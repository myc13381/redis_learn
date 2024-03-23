#include "dict.h"


size_t bitwise_hash(const char* first, size_t count)
{
#if (_MSC_VER && _WIN64) || ((__GNUC__ || __clang__) &&__SIZEOF_POINTER__ == 8) // 64位操作系统
  const size_t fnv_offset = 14695981039346656037ull;
  const size_t fnv_prime = 1099511628211ull;
#else
  const size_t fnv_offset = 2166136261u;
  const size_t fnv_prime = 16777619u;
#endif
  size_t result = fnv_offset;
  for (size_t i = 0; i < count; ++i)
  {
    result ^= (size_t)first[i];
    result *= fnv_prime;
  }
  return result;
}
Hashtable::Hashtable(int baseNum)
{
    if (baseNum <= 0)
        baseNum = 2;
    _bucketSize = 1ul << baseNum;
    _sizeMask = _bucketSize - 1;
    _nodeSize = 0;
    _mlf = 3.0f;
    _buckets.resize(_bucketSize, nullptr);
}

Hashtable::~Hashtable()
{
    clear();
}

size_t Hashtable::hash(const std::string &key)
{
    return bitwise_hash(key.c_str(), key.length() * sizeof(key[0])) % _sizeMask;
}

// 查找，找不到返回nullptr，找到返回对应节点的指针
HashNode* Hashtable::find(const std::string key)
{
    size_t index = hash(key);
    HashNode *node = _buckets[index];
    while(node != nullptr)
    {
        if(node->getKey() == key)
            break;
        node=node->next();
    }
    return node;
}

void Hashtable::insert(std::string key, std::string value)
{
    
    HashNode *node = find(key);
    if(node == nullptr)
    {
        size_t index = hash(key);
        node = new HashNode();
        node->setKey(key);
        node->setValue(value);
        if(_buckets[index] != nullptr) node->next() = _buckets[index]->next();
        _buckets[index] = node;
        ++_nodeSize; // 增加一个节点的数量
    }
    else // key 已经存在 只需要修改 value 即可
    {
        node->setValue(value);
    }
}

// 删除元素 返回删除节点的指针，如果节点不存在，则返回nullptr，对应在堆区开辟的空间，需要程序员自己释放
HashNode* Hashtable::erase(const std::string key)
{
    size_t index = hash(key);
    HashNode *node = _buckets[index], *prev = nullptr;
    while(node != nullptr)
    {
        prev = node;
        if(node->getKey() == key)
            break;
        node = node->next();
    }
    if(node == nullptr) return nullptr;
    if(prev == _buckets[index]) _buckets[index] = node->next();
    else prev->next() = node->next();
    --_nodeSize;
    return node;
}

void Hashtable::clear()
{
    HashNode *node = nullptr;
    HashNode *temp = nullptr;
    for(int i=0;i<_bucketSize;++i)
    {
        node = _buckets[i];
        while(node != nullptr)
        {
            temp = node->next();
            delete node;
            node = temp;
        }
        _buckets[i] = nullptr;
    }
    _nodeSize = 0;
}

void Hashtable::unsafeResize(size_t newSize)
{
    clear();
    _bucketSize = newSize;
    _nodeSize = 0;
    _sizeMask = _bucketSize - 1;
    _buckets.resize(newSize, nullptr);
}

int Hashtable::rehash_if_need(size_t n)
{
    if(_nodeSize + n > (static_cast<float>(_bucketSize) * _mlf)) return 1;
    else if(_nodeSize + n < _bucketSize / 2) return -1;
    else return 0;
}

Dict::Dict(int baseNum) : _rehashIdx(nops), _iterators(0)
{
    _hashtable[0] = Hashtable(baseNum);
}

HashNode* Dict::find(std::string key)
{
    HashNode *node = _hashtable[0].find(key);
    if(_rehashIdx == nops || node != nullptr)
    { // 不在 rehash, 或者在第一个表中已经找到
        return node;
    }
    else
    { // 在 rehash , 并且在第一个 表中没有找到，因此在第二个表中寻找
        node = _hashtable[1].find(key);
        return node;
    }
}


void Dict::insert(std::string key, std::string value)
{
    HashNode *node = nullptr;
    // 先在第一个表中寻找
    size_t index = _hashtable[0].hash(key);
    node = _hashtable[0].getBucket()[index];
    while(node != nullptr)
    {
        if(node->getKey() == key) break;
        node = node->next();
    }
    if(node != nullptr)
    { // 在第一个表中找到了
        node->setValue(value);
        return;
    }
    if(_rehashIdx == nops)
    { // 没有 rehash，直接在第一个表插入
        node = new HashNode();
        node->setKey(key);
        node->setValue(value);
        if(_hashtable[0].getBucket()[index] != nullptr) node->next() = _hashtable[0].getBucket()[index]->next();
        _hashtable[0].getBucket()[index] = node;
        ++_hashtable[0].nodeSize();
        return;
    }
    // 此时，处于 rehash 状态，并且第一个表中没有找到，在第二个表中进行操作
    index = _hashtable[1].hash(key);
    node = _hashtable[1].getBucket()[index];
    while(node != nullptr)
    { // 在第二个表中找到key，直接修改value
        if(node->getKey() == key) break;
        node = node->next();
    }
    if(node != nullptr)
    {
        node->setValue(value);
    }
    else
    {   // 插入新节点
        node = new HashNode();
        node->setKey(key);
        node->setValue(value);
        if(_hashtable[1].getBucket()[index] != nullptr) node->next() = _hashtable[1].getBucket()[index]->next();
        _hashtable[1].getBucket()[index] = node;
        ++_hashtable[1].nodeSize();
    }
    return;
}

HashNode* Dict::erase(std::string key)
{
    HashNode *node = nullptr, *prev = nullptr;
    size_t index = _hashtable[0].hash(key);
    prev = node = _hashtable[0].getBucket()[index];
    while(node != nullptr)
    {
        prev = node;
        if(node->getKey() == key) break;
        node = node->next();
    }
    if(node != nullptr)
    {
        if(prev == _hashtable[0].getBucket()[index])
        {
            _hashtable[0].getBucket()[index] = node->next();
        }
        else prev->next() = node->next();
        --_hashtable[0].nodeSize();
        return node;
    }
    // 第一个表中没有找到key，如果需要，在第二个表中寻找
    if(_rehashIdx != nops)
    {
        index = _hashtable[1].hash(key);
        prev = node = _hashtable[1].getBucket()[index];
        while(node != nullptr)
        {
            if(node->getKey() == key) break;
            prev = node;
            node = node->next();
        }
        if(node != nullptr)
        {
            if(prev == _hashtable[1].getBucket()[index])
            {
                _hashtable[1].getBucket()[index] = prev;
            }
            else prev->next() = node->next();
            --_hashtable[1].nodeSize();
        }
    }
    return node; // node 为对应节点或者 nullptr
}

int Dict::rehash(int n)
{
    if(_rehashIdx == nops) // 如果还没有开始rehash
    {
        int ret = _hashtable[0].rehash_if_need(0);
        int nodeSize = _hashtable[0].nodeSize();
        if(ret == 0) return 0;
        else if(ret == -1 && nodeSize > 128) _hashtable[1].unsafeResize(nodeSize / 2); // 重新设置 _hashtable[1] 的长度
        else _hashtable[1].unsafeResize(nodeSize * 2);
        _rehashIdx = 0; // rehash 初始化 _rehashIdx = 0
    } 
    int empty_visits = n * 10;

    while(n-- && !_hashtable[0].empty())
    {
        HashNode *node = nullptr, *next = nullptr;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        while(_hashtable[0].getBucket()[_rehashIdx]->next() == nullptr)
        {
            ++_rehashIdx;
            if(--empty_visits == 0) return 1;
        }

        node = _hashtable[0].getBucket()[_rehashIdx]->next();
        while(node != nullptr)
        {   // 进行重哈希
            next = node->next();
            size_t index = _hashtable[1].hash(node->getKey());
            node->next() = _hashtable[1].getBucket()[index]->next();
            _hashtable[1].getBucket()[index]->next() = node;
            node = next;
            --_hashtable[0].nodeSize();
            ++_hashtable[1].nodeSize();
        }
        ++_rehashIdx;
    }

    // 判断是否 rehash 完毕
    if(_hashtable[0].empty())
    { // rehash 完毕
        // 进行交换
        std::swap(_hashtable[0],_hashtable[1]);
        _hashtable[0].clear();
        return 0;
    }

    // 还需要继续 rehash
    return 1;
}

int Dict::rehashMilliseconds(int64_t ms)
{
    if(!isRehashing()) return 0;
    auto start = std::chrono::system_clock::now().time_since_epoch().count();
    int rehashes = 0;
    while(rehash(100))
    {
        rehashes += 100;
        if(std::chrono::system_clock::now().time_since_epoch().count() - start > ms) break;
    }
    return rehashes;
}


void Dict::clear(std::function<void(void)> callback)
{
    int count = 0;
    HashNode *node = nullptr, *next = nullptr;
    for(int i=0;i<_hashtable[0].bucketSize();++i,++count)
    {
        node = _hashtable[0].getBucket()[i];
        while(node != nullptr)
        {
            next = node->next();
            delete node;
            node = next;
        }
        if(callback && count >= 65535)
        {
            callback();
            count = 0;
        }
    }
}

// 获取node的下一个节点，重哈希情况下不允许调用
HashNode* Dict::next(HashNode *node, size_t idx)
{
    return node->next();
}

// 返回第一个节点
HashNode* Dict::first()
{
    HashNode *node = nullptr;
    for(int i = 0;i<_hashtable[0].bucketSize();++i)
    {
        if(_hashtable[0].getBucket()[i] != nullptr)
        {
            node = _hashtable[0].getBucket()[i];
            break;
        }
    }
    return node;
}
// 返回尾部
HashNode* Dict::end()
{
    return nullptr;
}

void Dict::dump_file(const std::string &fileName)
{

}
void Dict::load_file(const std::string &fileName)
{
    
}
    



