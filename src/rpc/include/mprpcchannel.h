#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <algorithm>
#include <algorithm>  // 包含 std::generate_n() 和 std::generate() 函数的头文件
#include <functional>
#include <iostream>
#include <map>
#include <random>  // 包含 std::uniform_int_distribution 类型的头文件
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include"rpcheader.pb.h"
#include "util.h"



/// @brief 客户端请求发送，维护tcp连接，并发送请求
class MprpcChannel :public google::protobuf::RpcChannel {
public:

    //header_size + service_name method_name args_size + args
    // 所有通过stub代理对象调用的rpc方法，都会走到这里了，
    // 统一通过rpcChannel来调用方法        
    // 统一做rpc方法调用的数据数据序列化和网络发送
    void CallMethod(const google::protobuf::MethodDescriptor* method, google::protobuf::RpcController* controller,
        const google::protobuf::Message* request, google::protobuf::Message* response,
        google::protobuf::Closure* done) override {
        //  检查连接，没有则建立
        if (clientFd_ == -1) {
            std::string errMsg;
            bool rt = newConnect(ip_.c_str(), port_, &errMsg);
            if (!rt) {
                DPrintf("[func-MprpcChannel::CallMethod]重连接ip：{%s} port{%d}失败", ip_.c_str(), port_);
                controller->SetFailed(errMsg);
                return;
            } else {
                DPrintf("[func-MprpcChannel::CallMethod]连接ip：{%s} port{%d}成功", ip_.c_str(), port_);
            }
        }

        //服务信息
        const google::protobuf::ServiceDescriptor* sd = method->service();
        std::string service_name = sd->name();     // service_name
        std::string method_name = method->name();  // method_name
        // 获取参数的序列化字符串长度 args_size
        uint32_t args_size{};
        std::string args_str;
        if (request->SerializeToString(&args_str)) {
            args_size = args_str.size();
        } else {
            controller->SetFailed("serialize request error!");
            return;
        }

        // 构建请求
        RPC::RpcHeader rpcHeader;
        rpcHeader.set_service_name(service_name);
        rpcHeader.set_method_name(method_name);
        rpcHeader.set_args_size(args_size);

        // 请求转字符串
        std::string rpc_header_str;
        if (!rpcHeader.SerializeToString(&rpc_header_str)) {
            controller->SetFailed("serialize rpc header error!");
            return;
        }


        // 使用protobuf的CodedOutputStream来构建发送的数据流
        std::string send_rpc_str;  // 用来存储最终发送的数据
        {
            // 创建一个StringOutputStream用于写入send_rpc_str
            google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
            google::protobuf::io::CodedOutputStream coded_output(&string_output);

            // 先写入header的长度（变长编码）
            coded_output.WriteVarint32(static_cast<uint32_t>(rpc_header_str.size()));

            // 不需要手动写入header_size，因为上面的WriteVarint32已经包含了header的长度信息
            // 然后写入rpc_header本身
            coded_output.WriteString(rpc_header_str);
        }


        // 最后，将请求参数附加到send_rpc_str后面
        send_rpc_str += args_str;


        // 发送rpc请求
        //失败会重试连接再发送，重试连接失败会直接return
        while (-1 == send(clientFd_, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
            char errtxt[512] = { 0 };
            sprintf(errtxt, "send error! errno:%d", errno);
            std::cout << "尝试重新连接，对方ip：" << ip_ << " 对方端口" << port_ << std::endl;
            close(clientFd_);
            clientFd_ = -1;
            std::string errMsg;
            bool rt = newConnect(ip_.c_str(), port_, &errMsg);
            if (!rt) {
                controller->SetFailed(errMsg);
                return;
            }
        }


        //从时间节点来说，这里将请求发送过去之后rpc服务的提供者就会开始处理，返回的时候就代表着已经返回响应了


        // 接收rpc请求的响应值
        char recv_buf[1024] = { 0 };
        int recv_size = 0;
        if (-1 == (recv_size = recv(clientFd_, recv_buf, 1024, 0))) {
            close(clientFd_);
            clientFd_ = -1;
            char errtxt[512] = { 0 };
            sprintf(errtxt, "recv error! errno:%d", errno);
            controller->SetFailed(errtxt);
            return;
        }

        // 反序列化rpc调用的响应数据
        // std::string response_str(recv_buf, 0, recv_size); //
        // bug：出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败 if
        // (!response->ParseFromString(response_str))
        if (!response->ParseFromArray(recv_buf, recv_size)) {
            char errtxt[1050] = { 0 };
            sprintf(errtxt, "parse error! response_str:%s", recv_buf);
            controller->SetFailed(errtxt);
            return;
        }
    }
    MprpcChannel(std::string ip, short port, bool connectNow) :ip_(ip), port_(port), clientFd_(-1) {
        // 使用tcp编程，完成rpc方法的远程调用，使用的是短连接，因此每次都要重新连接上去，待改成长连接。
        // 没有连接或者连接已经断开，那么就要重新连接呢,会一直不断地重试
        // 读取配置文件rpcserver的信息
        // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
        // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
        // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
        //  /UserServiceRpc/Login

        //可以允许延迟连接
        if (!connectNow) {
            return;
        }

        std::string errMsg;
        auto rt = newConnect(ip.c_str(), port, &errMsg);
        int tryCount = 3;
        while (!rt && tryCount--) {
            std::cout << errMsg << std::endl;
            rt = newConnect(ip.c_str(), port, &errMsg);
        }
    }
private:
    int clientFd_;
    const std::string ip_;
    const uint16_t port_;
    bool newConnect(const char* ip, uint16_t port, std::string* errMsg) {

        // 用 C / C++ 的 ​​Berkeley 套接字 API​​ 创建一个 TCP 客户端套接字：
        int clientfd = socket(AF_INET, SOCK_STREAM, 0);
        // 创建失败
        if (-1 == clientfd) {
            char errtxt[512] = { 0 };
            sprintf(errtxt, "create socket error! errno:%d", errno);
            clientFd_ = -1;
            *errMsg = errtxt;
            return false;
        }
        struct sockaddr_in server_addr;               // 定义 IPv4 地址结构体
        server_addr.sin_family = AF_INET;             // 设置地址族为 IPv4
        server_addr.sin_port = htons(port);           // 设置端口号（转换为网络字节序）
        server_addr.sin_addr.s_addr = inet_addr(ip);  // 将字符串 IP 转换为网络字节序


        // 连接rpc服务节点
        // 建立 TCP 连接​​，并检查 connect() 是否成功。如果连接失败（返回 -1），通常会进行错误处理。
        // clientfd：客户端套接字描述符（由 socket() 创建）。
        // (struct sockaddr*)&server_addr：指向服务器地址结构体的指针（需强制转换）。
        // sizeof(server_addr)：服务器地址结构体的大小。
        // connect() 连接服务器时​​
        // ​​系统自动分配一个临时端口（Ephemeral Port）​​，范围通常是 ​​32768~60999​​（Linux 默认）。
        // 这个端口会在连接建立期间被占用，直到调用 close() 关闭套接字。
        if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
            close(clientfd);
            char errtxt[512] = { 0 };
            sprintf(errtxt, "connect fail! errno:%d", errno);
            clientFd_ = -1;
            *errMsg = errtxt;
            return false;
        }
        clientFd_ = clientfd;
    }
};