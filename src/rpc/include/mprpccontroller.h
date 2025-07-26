#pragma once
#include <google/protobuf/service.h>
#include <string>

class MprpcController : public google::protobuf::RpcController {
public:
    MprpcController() :m_failed(false), m_errText("") { }
    void Reset() { m_failed = false; m_errText = ""; }
    bool Failed() const override { return m_failed; }
    std::string ErrorText() const override { return m_errText; }
    void SetFailed(const std::string& reason) override {
        m_failed = true;
        m_errText = reason;
    }

    // 目前未实现具体的功能
    void StartCancel() { }
    bool IsCanceled() const { }
    void NotifyOnCancel(google::protobuf::Closure* callback) { }

private:
    bool m_failed;          // RPC方法执行过程中的状态
    std::string m_errText;  // RPC方法执行过程中的错误信息
};