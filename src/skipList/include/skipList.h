#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>


#define STORE_FILE "store/dumpFile"

static std::string delimiter = ":";
// skiplist node
template <typename K, typename V>
class Node {
public:
    Node() { }
    Node(K k, V v, int);
    ~Node();

    K get_key()const;
    V get_value() const;
    void set_value(V);
    Node<K, V>** forward;

    int node_level;
private:
    K key;
    V value;
};

template <typename K, typename V>
Node<K, V>::Node(const K k, const V v, int level) :key(k), value(v), node_level(level) {

    this->forward = new Node<K, V>* [level + 1];
    memset(this->forward, 0, sizeof(Node<K, V>*) * (level + 1));
};

template <typename K, typename V>
Node<K, V>::~Node() {
    delete[] forward;
};

template <typename K, typename V>
K Node<K, V>::get_key()const {
    return key;
};

template <typename K, typename V>
V Node<K, V>::get_value() const {
    return value;
};

template <typename K, typename V>
void Node<K, V>::set_value(V value) {
    this->value = value;
};

template <typename K, typename V>
class SkipListDump {
public:
    friend class boost::serialization::access;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar& keyDumpVt_;
        ar& valDumpVt_;
    }
    std::vector<K> keyDumpVt_;
    std::vector<V> valDumpVt_;

    void insert(const Node<K, V>& node) {
        keyDumpVt_.emplace_back(node.get_key());
        valDumpVt_.emplace_back(node.get_value());
    }
};


template <typename K, typename V>
class SkipList {
public:
    SkipList(int);
    ~SkipList();
    int get_random_level();
    Node<K, V>* create_node(K, V, int);
    int insert_element(K, V);
    void display_list();
    bool search_element(K, V& value);
    void delete_element(K);
    void insert_set_element(K&, V&);
    std::string dump_file();
    void load_file(const std::string& dumpStr);
    void clear(Node<K, V>*);
    int size();
private:
    void get_key_value_from_string_(const std::string& str, std::string* key, std::string* value);
    bool is_valid_string_(const std::string& str);


    // Maximum level of the skip list
    int max_level_;
    // current level of the skip list;
    int skip_list_level_;
    // pointer to header node
    Node<K, V>* header_;

    // file operator
    std::ofstream file_writer_;
    std::ifstream file_reader_;

    // the num of current skip list
    int element_count_;

    // mutex 
    std::mutex mtx_;
};

// construct skip list
template <typename K, typename V>
SkipList<K, V>::SkipList(int max_level) {
    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
};

template <typename K, typename V>
SkipList<K, V>::~SkipList() {
    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }

    //递归删除跳表链条
    if (_header->forward[0] != nullptr) {
        clear(_header->forward[0]);
    }
    delete (_header);
}

template <typename K, typename V>
void SkipList<K, V>::clear(Node<K, V>* cur) {
    if (cur->forward[0] != nullptr) {
        clear(cur->forward[0]);
    }
    delete (cur);
}

// create new node
template <typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int level) {
    Node<K, V>* newNode = new Node<K, V>(k, v, level);
    return newNode;
}


// Display skip list
template <typename K, typename V>
void SkipList<K, V>::display_list() {
    std::cout << "\n*****Skip List*****"
        << "\n";
    for (int i = 0; i <= skip_list_level_; i++) {
        Node<K, V>* node = this->_header->forward[i];
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

template <typename K, typename V>
int SkipList<K, V>::get_random_level() {
    int k = 1;
    while (rand() % 2) {
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};

template <typename K, typename V>
int SkipList<K, V>::size() {
    return element_count_;
};

template <typename K, typename V>
void SkipList<K, V>::get_key_value_from_string_(const std::string& str, std::string* key, std::string* value) {
    if (!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter) + 1, str.length());
}

template <typename K, typename V>
bool SkipList<K, V>::is_valid_string_(const std::string& str) {
    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}

// 序列化和反序列化
// todo 对dump 和 load 后面可能要考虑加锁的问题
// Dump data in memory to file
template <typename K, typename V>
std::string SkipList<K, V>::dump_file() {
    // std::cout << "dump_file-----------------" << std::endl;
    //
    //
    // _file_writer.open(STORE_FILE);
    Node<K, V>* node = this->_header->forward[0];
    SkipListDump<K, V> dumper;
    while (node != nullptr) {
        dumper.insert(*node);
        // _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        // std::cout << node->get_key() << ":" << node->get_value() << ";\n";
        node = node->forward[0];
    }
    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);
    oa << dumper;
    return ss.str();
    // _file_writer.flush();
    // _file_writer.close();
}

// Load data from disk
template <typename K, typename V>
void SkipList<K, V>::load_file(const std::string& dumpStr) {
    // _file_reader.open(STORE_FILE);
    // std::cout << "load_file-----------------" << std::endl;
    // std::string line;
    // std::string* key = new std::string();
    // std::string* value = new std::string();
    // while (getline(_file_reader, line)) {
    //     get_key_value_from_string(line, key, value);
    //     if (key->empty() || value->empty()) {
    //         continue;
    //     }
    //     // Define key as int type
    //     insert_element(stoi(*key), *value);
    //     std::cout << "key:" << *key << "value:" << *value << std::endl;
    // }
    // delete key;
    // delete value;
    // _file_reader.close();

    if (dumpStr.empty()) {
        return;
    }
    SkipListDump<K, V> dumper;
    std::stringstream iss(dumpStr);
    boost::archive::text_iarchive ia(iss);
    ia >> dumper;
    for (int i = 0; i < dumper.keyDumpVt_.size(); ++i) {
        insert_element(dumper.keyDumpVt_[i], dumper.keyDumpVt_[i]);
    }
}


template <typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    std::unique_lock<std::mutex> locker(mtx_);
    locker.lock();

    Node<K, V>* current = header_;

    Node<K, V>* update[max_level_ + 1];
    memset(update, 0, sizeof(Node<K, V>) * (max_level_ + 1));

    for (int i = skip_list_level_;i >= 0;i--) {
        while (current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];

    if (current && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        locker.unlock();
        return 1;
    }
    int random_level = get_random_level();
    if (random_level > skip_list_level_) {
        for (int i = skip_list_level_ + 1;i < random_level;i++) {
            update[i] = header_;
        }
        skip_list_level_ = random_level;
    }

    Node<K, V>* insert_node = create_node(key, value, random_level);
    for (int i = 0;i < random_level;i++) {
        insert_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = insert_node;
    }
    std::cout << "insert key: " << key << ", value: " << value << std::endl;

    element_count_++;

    locker.unlock();
    return 0;
}

// Delete element from skip list
template <typename K, typename V>
void SkipList<K, V>::delete_element(K key) {
    mtx_.lock();
    Node<K, V>* current = header_;
    Node<K, V>* update[max_level_ + 1];
    memset(update, 0, sizeof(Node<K, V> *) * (max_level_ + 1));

    // start from highest level of skip list
    for (int i = skip_list_level_; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    if (current != NULL && current->get_key() == key) {
        // start for lowest level and delete the current node of each level
        for (int i = 0; i <= skip_list_level_; i++) {
            // if at level i, next node is not target node, break the loop.
            if (update[i]->forward[i] != current) break;

            update[i]->forward[i] = current->forward[i];
        }

        // Remove levels which have no elements
        while (skip_list_level_ > 0 && header_->forward[skip_list_level_] == 0) {
            skip_list_level_--;
        }

        std::cout << "Successfully deleted key " << key << std::endl;
        delete current;
        _element_count--;
    }
    _mtx.unlock();
    return;
}

/**
 * \brief 作用与insert_element相同类似，
 * insert_element是插入新元素，
 * insert_set_element是插入元素，如果元素存在则改变其值
 */
template <typename K, typename V>
void SkipList<K, V>::insert_set_element(K& key, V& value) {
    V oldValue;
    if (search_element(key, oldValue)) {
        delete_element(key);
    }
    insert_element(key, value);
}

template <typename K, typename V>
bool SkipList<K, V>::search_element(K key, V& value) {
    std::cout << "search_element-----------------" << std::endl;
    Node<K, V>* current = _header;

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    // reached level 0 and advance pointer to right node, which we search
    current = current->forward[0];
    // if current node have key equal to searched key, we get it
    if (current and current->get_key() == key) {
        value = current->get_value();
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}