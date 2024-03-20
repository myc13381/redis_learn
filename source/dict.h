#ifndef REDIS_LEARN_DICT
#define REDIS_LEARN_DICT

// 实现数据结构   字典
// 使用两个哈希表实现渐进式重哈希

#include <vector>
#include <string>
#include <functional>
#include <chrono>

/*
 * FNV哈希算法是一种非加密的哈希算法，全名为Fowler-Noll-Vo算法。它以三位发明人Glenn Fowler，Landon Curt Noll，Phong Vo的名字命名，最早在1991年提出。
 * FNV哈希算法的特点是能快速对大量数据进行哈希处理，并保持较小的冲突率。它的高度分散性使其特别适用于对非常相近的字符串进行哈希，如URL、hostname、文件名、text和IP地址等。
 * 
*/
// 逐位哈希
inline size_t bitwise_hash(const char* first, size_t count);

constexpr size_t nops = static_cast<size_t>(-1); // 表示 size_t 的最大值

// DictEntry
class HashNode;
class Hashtable;
class Dict;

class HashNode
{
public:
    HashNode() : _next(nullptr) {}
    HashNode(std::string key, std::string value) : _key(std::move(key)), _value(std::move(value)), _next(nullptr) {}
    void setKey(std::string &str) { _key = str; }
    void setValue(std::string &str) { _value = str; }
    std::string getKey() const {return _key;}
    std::string getValue() const {return _value;}
    HashNode*& next() {return _next;}
private:
    std::string _key;
    std::string _value;
    HashNode *_next; // 指向下一个 entry 的指针
};

#define DEFAULT_BUCKTNUM 128

class Hashtable
{
public:
    Hashtable() : _bucketSize(DEFAULT_BUCKTNUM), _nodeSize(0), _sizeMask(_bucketSize-1), _mlf(3.0f), _buckets(DEFAULT_BUCKTNUM, nullptr) {}
    Hashtable(int baseNum);
    ~Hashtable(); // 析构函数
    // 哈希函数
    size_t hash(const std::string &key);

    HashNode* find(const std::string &key);
    void insert(std::string &key, std::string &value);
    HashNode* erase(const std::string &key);
    void clear();
    bool empty() { return _nodeSize == 0; }
    // 不安全的扩/缩容 仅限于在 Dict 的 rehash 中进行调用，newSize 需要保证是2的N次方
    // 该函数会清空整个哈希表
    void unsafeResize(size_t newSize);
    // 判断是否需要 rehash，不需要返回0，需要扩容返回1，需要缩容返回 -1
    int rehash_if_need(size_t n);
    // 获取桶
    std::vector<HashNode*>& getBucket() { return _buckets; };
    size_t& nodeSize() { return _nodeSize; }
    size_t bucketSize() { return _bucketSize; }
private:
    std::vector<HashNode *> _buckets; // 存放桶的数组
    size_t _bucketSize; // 桶的数目
    size_t _nodeSize; // 节点的数目

    size_t _sizeMask; // 用于扩容时取余

    float _mlf; // 最大负载

};

class Dict
{
public:
    Dict() : _rehashIdx(nops), _iterators(0) {}
    Dict(int baseNum);
    ~Dict() {}
    HashNode* find(std::string &key);
    void insert(std::string &key, std::string &value);
    HashNode* erase(std::string &key);
    // 重哈希 n 个桶中的位置，rehash 完毕返回0，否则返回1
    int rehash(int n);
    // 在 ms 毫秒内持续 rehash, 单次 rehash 至少100个元素
    int rehashMilliseconds(int64_t ms);
    // clear 必须一次执行完毕
    // callback 用于在可能出现的长时间阻塞情况下进行一些必要操作，例如集群中发送定时消息
    void clear(std::function<void(void)> callback);

    size_t& rehashIdx() { return _rehashIdx; }
    size_t& iterators() { return _iterators; }
    Hashtable* getTable() { return _hashtable;}
    bool empty() { return (_hashtable[0].empty() && _hashtable[1].empty()); }

private:
    size_t _rehashIdx; // 重哈希的索引，每次重哈希的单位是桶的一个元素，如果 rehashIdx == nops ,代表没有在rehash
    size_t _iterators; // 安全迭代器的数目
    Hashtable _hashtable[2]; // hashtable
};

#endif