#ifndef REDIS_LEARN_HYPERLOGLOG
#define REDIS_LEARN_HYPERLOGLOG

#include <string>
#include <vector>
#include <cmath>
#include <cassert>

#define BYTE_ORDER __BYTE_ORDER
constexpr uint8_t HLL_DENSE = 0; /* 稠密编码 */
constexpr uint8_t HLL_SPARSE = 1;/* 稀疏编码 */
constexpr uint8_t HLL_RAW = 255; /* 内部编码 */
constexpr uint8_t HLL_MAX_ENCODING = 1;
constexpr size_t HLL_P = 14; /* hash值的后14位用于记录索引 */
constexpr size_t HLL_Q = 64 - HLL_P; /* 50 最多连续49个0 */
constexpr size_t HLL_REGISTERS = (1 << HLL_P); /* 16384 分组的最大数量 */
constexpr size_t HLL_P_MASK = HLL_REGISTERS - 1; /* 0011111111111111 */
constexpr size_t HLL_BITS = 6; /* kmax=64 2^64 */
constexpr size_t HLL_REGISTER_MAX = (1 << HLL_BITS) -1; /* 00111111 63 每一个register能存放的最大值*/


/* 定义 hllhdr */
struct Hllder
{
    char magic[4]; /* HYLL */
    uint8_t encoding; /* HLL_DENSE-->0 HLL_SPARSE-->1 */
    uint8_t notused[3]; /* 未使用的区域,必须置零 */
    uint8_t card[8]; /* 缓存最近的基数,小端模式 MSB(最高有效位)用来标志card是否有效，0->有效，1->无效*/
    Hllder():encoding(HLL_SPARSE)
    {
        magic[0]='H';
        magic[1]='Y';
        magic[2]='L';
        magic[3]='L';
    }

    // 将card设置位无效
    void hllInvalidateCache()
    {
        card[7] |= (1<<7);
    }

    // 判断card是否有效 true有效，false无效
    bool hllValidCache() const
    {
        return card[7] & (1<<7) == 0;
    }
};

constexpr size_t HLL_HDR_SIZE = sizeof(Hllder); 
constexpr size_t HLL_DENSE_SIZE = (HLL_HDR_SIZE + (HLL_REGISTERS * HLL_BITS + 7)/8); /* 一个HLL_DENSE的大小=头部的大小+(分组个数*分组大小+7)/8 单位byte */
static std::string invalid_hll_err("-INVALIDOBJ Corrupted HLL object detected");
/* 稀疏编码，以字节为单位 */
constexpr uint8_t HLL_SPARSE_XZEOR_BIT = 0x40; /* 01xxxxxx */
constexpr uint8_t HLL_SPARSE_VAL_BIT = 0x80; /* 1vvvvvxx */
/* 定义最大长度和最大值 */
constexpr uint8_t HLL_SPARSE_VAL_MAX_VALUE = 32;
constexpr uint8_t HLL_SPARSE_VAL_MAX_LEN = 4;
constexpr uint8_t HLL_SPARSE_ZERO_MAX_LEN = 64;
constexpr uint8_t HLL_SPARSE_XZERO_MAX_LEN = 16384;
constexpr double HLL_ALPHA_INF = 0.721347520444481703680f; /* constant for 0.5/ln(2) */



/* 类HyperLogLog */
class HyperLogLog
{
public:
    HyperLogLog():hllder(Hllder()),registers(std::vector<uint8_t>({0x7f,0xff})),reghisto(std::vector<int>(64,0)) {}
    /* ===稠密编码相关操作=== */

    /* 得到index位置的register的值 */
    inline uint8_t hllDenseGetRegister(const std::vector<uint8_t> &reg, size_t index);

    /* 设置index位置的register的值 */
    static inline void hllDenseSetRegister(std::vector<uint8_t> &reg, size_t index, uint8_t value);

    /* ===稀疏编码相关操作=== */

    /* 判断是哪种操作吗 */
    static inline bool hllSparseIsZero(const uint8_t &opcode);
    static inline bool hllSparseIsXzero(const uint8_t &opcode);
    static inline bool hllSparseIsVal(const uint8_t &opcode);

    /* 获取数量和值 */
    inline uint8_t hllSparseZeroLen(size_t index) const;
    inline uint16_t hllSparseXzeroLen(size_t index) const;
    inline uint8_t hllSparseValLen(size_t index) const;
    inline uint8_t hllSparseValValue(size_t index) const;

    /* 设置值 */
    static inline void hllSparseValSet(std::vector<uint8_t> &regs, size_t &index, uint8_t value, uint8_t len);
    static inline void hllSparseZeroSet(std::vector<uint8_t> &regs, size_t &index, uint8_t len);
    static inline void hllSparseXzeroSet(std::vector<uint8_t> &regs, size_t &index, uint16_t len);

    /* ===HyperLogLog algorithm=== */
    /* 这是一个使用MurmurHash2算法的64位哈希函数的实现。该算法是为Redis修改的，以在大端和小端架构上提供相同的结果，使其具有端序中立性。 */
    static uint64_t MurmurHash64A(const void * key, int len, unsigned int seed);

    /* 根据给定的键计算索引并返回含有多少个连续的0 */
    static int hllPatLen(std::string &key, size_t &index);

    /* 设置index的值，如果register[index]的值小于count，则更新并返回1，否则不更新返回0 */
    int hllDenseSet(std::vector<uint8_t> &regs, size_t &index, uint8_t count);

    /* "添加"元素 */
    int hllDenseAdd(std::vector<uint8_t> &regs, std::string &elem);

    /* Compute the register histogram in the dense representation. */
    void hllDenseRegHisto();

    /* 从稀疏编码转换到密集编码 返回1表示成功，0表示失败 */
    int hllSparseToDense();

    /* 稀疏编码设置值 返回最后的长度*/
    int hllSparseSet(size_t &index, uint8_t count);

    /* 优化registers合并同类项 */
    void hllUpdateRegisters(std::vector<uint8_t> &regs,size_t start);

    /* 稀疏编码添加元素 */
    int hllSparseAdd(std::vector<uint8_t> &regs, std::string &elem);

    /* Compute the register histogram in the sparse representation. */
    bool hllSparseRegHisto();

    /* ===HyperLogLog Count=== */

    /* Compute the register histogram in the raw representation. */
    inline void hllRawRegHisto();

    double hllSigma(double x);

    double hllTau(double x);

    /* 基数统计 */
    uint64_t hllCount(int &invalid);

    /* Call hllDenseAdd() or hllSparseAdd() according to the HLL encoding. */
    int hllAdd(std::string &elem);

    /* merge */
    int hllMerge(std::vector<uint8_t> &reg);
    
private:
    Hllder hllder;
    std::vector<uint8_t> registers;
    std::vector<int> reghisto;
};

inline uint8_t HyperLogLog::hllDenseGetRegister(const std::vector<uint8_t> &reg, size_t index)
{
    size_t _byte = index * HLL_BITS / 8;
    size_t _fb =  index * HLL_BITS & 7;
    size_t _fb8 = 8- _fb;
    uint8_t b0 = reg[_byte];
    uint8_t b1 = reg[_byte + 1];
    return static_cast<uint8_t>(((b0 >> _fb) | (b1 << _fb8)) & HLL_REGISTER_MAX);
}

inline void HyperLogLog::hllDenseSetRegister(std::vector<uint8_t> &reg, size_t index, uint8_t value)
{
    size_t _byte = index * HLL_BITS / 8;
    size_t _fb =  index * HLL_BITS & 7;
    size_t _fb8 = 8- _fb;
    reg[_byte] &= ~(HLL_REGISTERS << _fb);
    reg[_byte] |= value << _fb;
    reg[_byte + 1] &= ~(HLL_REGISTERS >> _fb8);
    reg[_byte + 1] |= value >> _fb8;
}

/* 判断是哪种操作吗 */
inline bool HyperLogLog::hllSparseIsZero(const uint8_t &opcode)
{
    return (opcode & 0xc0) == 0;
}
inline bool HyperLogLog::hllSparseIsXzero(const uint8_t &opcode)
{
    return (opcode & 0xc0) == HLL_SPARSE_XZEOR_BIT;
}

inline bool HyperLogLog::hllSparseIsVal(const uint8_t &opcode)
{
    return (opcode & HLL_SPARSE_VAL_BIT);
}

/* 获取数量和值 */
inline uint8_t HyperLogLog::hllSparseZeroLen(size_t index) const
{
    return (this->registers[index] & 0x3f)+1;
}

inline uint16_t HyperLogLog::hllSparseXzeroLen(size_t index) const
{
    return ((this->registers[index] & 0x3f) << 8) + (this->registers[index+1])+1;
}

inline uint8_t HyperLogLog::hllSparseValLen(size_t index) const
{
    return (this->registers[index] & 0x3)+1;
}

inline uint8_t HyperLogLog::hllSparseValValue(size_t index) const
{
    return ((this->registers[index] >> 2) & 0x1f)+1;
}

/* 设置值 */
inline void HyperLogLog::hllSparseValSet(std::vector<uint8_t> &regs, size_t &index, uint8_t value, uint8_t len)
{
    regs[index] = ((value-1) << 2 | (len-1)) | HLL_SPARSE_VAL_BIT;
}

inline void HyperLogLog::hllSparseZeroSet(std::vector<uint8_t> &regs, size_t &index, uint8_t len)
{
    regs[index] = len-1;
}

inline void HyperLogLog::hllSparseXzeroSet(std::vector<uint8_t> &regs, size_t &index, uint16_t len)
{
    len-=1;
    regs[index] = (len >> 8) | HLL_SPARSE_XZEOR_BIT;
    regs[index+1] = len & 0xff;
}

uint64_t HyperLogLog::MurmurHash64A(const void * key, int len, unsigned int seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint8_t *data = static_cast<const uint8_t *>(key);
    const uint8_t *end = data + (len - (len & 7));
    while(data != end)
    {
        uint64_t k = *(reinterpret_cast<const uint64_t *>(data));
#if (BYTE_ORDER == LITTLE_ENDIAN)

#else /* 将大端转化为小端 */
        k  = (uint64_t) data[0];
        k |= (uint64_t) data[1] << 8;
        k |= (uint64_t) data[2] << 16;
        k |= (uint64_t) data[3] << 24;
        k |= (uint64_t) data[4] << 32;
        k |= (uint64_t) data[5] << 40;
        k |= (uint64_t) data[6] << 48;
        k |= (uint64_t) data[7] << 56;
#endif
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
        data += 8;
    }

    switch(len & 7)
    {
        case 7: h ^= reinterpret_cast<const uint64_t *>(data)[6] << 48;
        case 6: h ^= reinterpret_cast<const uint64_t *>(data)[5] << 48;
        case 5: h ^= reinterpret_cast<const uint64_t *>(data)[4] << 48;
        case 4: h ^= reinterpret_cast<const uint64_t *>(data)[3] << 48;
        case 3: h ^= reinterpret_cast<const uint64_t *>(data)[2] << 48;
        case 2: h ^= reinterpret_cast<const uint64_t *>(data)[1] << 48;
        case 1: h ^= reinterpret_cast<const uint64_t *>(data)[0] << 48;
                h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}


int HyperLogLog::hllPatLen(std::string &key, size_t &index)
{
    uint64_t hash = MurmurHash64A(static_cast<const void *>(key.c_str()),key.size(),0xadc83b19ULL);
    index = hash & static_cast<size_t>(HLL_P_MASK); // hash的后14位作为索引
    hash >>= HLL_P;
    hash |= static_cast<uint64_t>(1 << HLL_Q);/* 将第 HLL_Q位置为1,确保循环能够终止 */
    int bit = 1, count = 1;
    while((hash & bit) == 0)
    {
        ++count;
        bit <<= 1;
    }
    return count;
}

int HyperLogLog::hllDenseSet(std::vector<uint8_t> &regs, size_t &index, uint8_t count)
{
    uint8_t oldCount = hllDenseGetRegister(regs,index);
    if(count > oldCount)
    {
        hllDenseSetRegister(regs,index,count);
        return 1;
    }
    return 0;
}

int HyperLogLog::hllDenseAdd(std::vector<uint8_t> &regs, std::string &elem)
{
    size_t index;
    int count = hllPatLen(elem, index);
    return hllDenseSet(regs, index, count);
}

void HyperLogLog::hllDenseRegHisto()
{
    if(HLL_REGISTERS == 16384 && HLL_BITS)
    {
        size_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15;
        int j=0;
        for(int i=0;i<1024;++i)
        {   // 循环展开，优化性能
            r0 = this->reghisto[0+j] & 63;
            r1 = (this->reghisto[0+j] >> 6 | this->reghisto[1+j] << 2) & 63;
            r2 = (this->reghisto[1+j] >> 4 | this->reghisto[2+j] << 4) & 63;
            r3 = (this->reghisto[2+j] >> 2) & 63;
            r4 = this->reghisto[3+j] & 63;
            r5 = (this->reghisto[3+j] >> 6 | this->reghisto[4+j] << 2) & 63;
            r6 = (this->reghisto[4+j] >> 4 | this->reghisto[5+j] << 4) & 63;
            r7 = (this->reghisto[5+j] >> 2) & 63;
            r8 =  this->reghisto[6+j] & 63;
            r9 = (this->reghisto[6+j] >> 6 | this->reghisto[7+j] << 2) & 63;
            r10 = (this->reghisto[7+j] >> 4 | this->reghisto[8+j] << 4) & 63;
            r11 = (this->reghisto[8+j] >> 2) & 63;
            r12 =  this->reghisto[9+j] & 63;
            r13 = (this->reghisto[9+j] >> 6 | this->reghisto[10+j] << 2) & 63;
            r14 = (this->reghisto[10+j] >> 4 | this->reghisto[11+j] << 4) & 63;
            r15 = (this->reghisto[11+j] >> 2) & 63;

            ++(this->reghisto[r0]);
            ++(this->reghisto[r1]);
            ++(this->reghisto[r2]);
            ++(this->reghisto[r3]);
            ++(this->reghisto[r4]);
            ++(this->reghisto[r5]);
            ++(this->reghisto[r6]);
            ++(this->reghisto[r7]);
            ++(this->reghisto[r8]);
            ++(this->reghisto[r9]);
            ++(this->reghisto[r10]);
            ++(this->reghisto[r11]);
            ++(this->reghisto[r12]);
            ++(this->reghisto[r13]);
            ++(this->reghisto[r14]);
            ++(this->reghisto[r15]);

            j+=12;
        }
    }
    else
    {
        for(int i=0;i<HLL_REGISTERS;++i)
        {
            ++(this->reghisto[hllDenseGetRegister(this->registers,i)]);
        }
    }
}

int HyperLogLog::hllSparseToDense()
{
    if(this->hllder.encoding == HLL_DENSE) return 1;
    this->hllder.encoding = HLL_DENSE;
    std::vector<uint8_t> dense(HLL_DENSE_SIZE,0);
    int len=this->registers.size();
    int p=0,runlen,idx=0,val;
    while(p<len)
    {
        if(hllSparseIsZero(this->registers[p]))
        {
            runlen=hllSparseZeroLen(p);
            idx += runlen;
            ++p;
        }
        else if(hllSparseIsXzero(this->registers[p]))
        {
            runlen=hllSparseXzeroLen(p);
            idx += runlen;
            p += 2;
        }
        else // val
        {
            runlen=hllSparseValLen(p);
            val=hllSparseValValue(p);
            if((runlen+idx)>HLL_REGISTERS) break;
            while(runlen--)
            {
                hllDenseSetRegister(dense,idx,val);
                ++idx;
            }
            ++p;
        }
    }
    if(idx != HLL_REGISTERS) return 0;
    std::swap(this->registers,dense);
    return 1;
}

int HyperLogLog::hllSparseSet(size_t &index, uint8_t count)
{
    if(count > 32)
    { // 超过32,直接进入稠密编码
        hllSparseToDense();
        hllSparseSet(index,count);
    }

    int end=this->registers.size();
    // 第一步，找到我们需要修改的操作码，并且确认是否真的要修改
    // first是我们目标操作码对应编码的第一个索引，prev是上一个操作码的索引，next是下一个操作码的索引
    size_t span=0; // 记录每个操作码内部包含了多少个操作
    size_t first=0,prev,next;
    size_t p=0; // 指针
    while(p<end)
    {
        size_t oplen = 1;
        if(hllSparseIsZero(this->registers[p]))
            span=hllSparseZeroLen(p);
        else if(hllSparseIsXzero(this->registers[p]))
        {
            span=hllSparseXzeroLen(p);
            oplen = 2;
        }
        else 
            span=hllSparseValLen(p);
        if(index <= first+span-1) break;
        prev=p;
        p+=oplen;
        first+=span;
    }
    
    if(span == 0 || p >= end) return -1; // 不合法的情况
    next = hllSparseIsXzero(p)?p+2:p+1;
    if(next >= end) next = 0;
    
    // 缓存数据
    bool is_zore=false,is_xzero=false,is_val=false;
    size_t runlen;
    if(hllSparseIsZero(p))
    {
        is_zore = true;
        runlen = hllSparseZeroLen(p);
    }
    else if(hllSparseIsXzero(p))
    {
        is_xzero = true;
        runlen = hllSparseXzeroLen(p);
    }
    else
    {
        is_val = true;
        runlen = hllSparseValLen(p);
    }

    // 第二步
    // A） 如果 VAL 操作码已设置为值 >= 我们的“计数”，则无论 VAL 运行长度字段如何，都不需要更新。在这种情况下，PFADD 返回 0，因为未执行任何更改。
    // B） 如果它是 len = 1 的 VAL 操作码（仅代表我们的寄存器）并且值小于 'count'，我们只需更新它，因为这是一个微不足道的情况。
    // C） 另一个需要处理的简单情况是len为1的ZERO操作码。我们可以用VAL操作码替换它，我们的值和len为1。
    // D） 其他情况更为复杂：我们的寄存器需要更新，目前由带有len>1的VAL操作码表示，由len>1的ZERO操作码表示，或者由XZERO操作码表示。
    //     在这些情况下，必须将原始操作码拆分为多个操作码。最坏的情况是中间的XZERO拆分导致XZERO--VAL--XZERO，因此生成的序列最大长度为5个字节。
    //     我们执行拆分，将新序列写入长度为“newlen”的“new”缓冲区。稍后，新序列将插入旧序列的位置，如果新序列比旧序列长，则可能会将右侧的内容移动几个字节。

    size_t oldCount;
    if(is_val)
    {
        // A)
        if(oldCount >= count) return 0; 
        // B）
        if(runlen == 1)
        {
            hllSparseValSet(this->registers,p,count,1);
            hllUpdateRegisters(this->registers,p);
        }
    }

    // C)
    if(is_zore && runlen == 1)
    {
        hllSparseValSet(this->registers,p,count,1);
        hllUpdateRegisters(this->registers,p);
    }

    // D)
    std::vector<uint8_t> temp(5,0);
    size_t ptr=0;//
    int last=first+span-1;
    int len;
    if(is_zore || is_xzero) // 目标操作码是ZERO 或和 XZERO
    {   
        // 处理前一部分的数据
        if(index != first)
        {
            len = index - first;
            if(len > HLL_SPARSE_ZERO_MAX_LEN) // 需要使用XZERO
            { 
                hllSparseXzeroSet(temp,ptr,len);
                ptr += 2;
            }
            else // 使用ZERO
            {
                hllSparseZeroSet(temp,ptr,len);
                ++ptr;
            }
        }
        hllSparseValSet(temp,ptr,count,1);
        ++ptr;
        // 处理后面一部分的数据
        if(index != last) 
        {
            {
                len = last - index;
                if(len > HLL_SPARSE_ZERO_MAX_LEN) // 需要使用XZERO
                { 
                    hllSparseXzeroSet(temp,ptr,len);
                    ptr += 2;
                }
                else // 使用ZERO
                {
                    hllSparseZeroSet(temp,ptr,len);
                    ++ptr;
                }
            }
        }
    }
    else // 目标操作码是VAL
    {
        int oldVal = hllSparseValValue(p);
        // 处理前面的数据
        if(index != first)
        {
            len = index - first;
            hllSparseValSet(temp,ptr,oldVal,len);
            ++ptr;
        }
        hllSparseValSet(temp,ptr,count,1);
        ++ptr;
        // 处理后面的数据
        if(index != last)
        {
            len = last - index;
            hllSparseValSet(temp,ptr,oldVal,len);
            ++ptr;
        }

    }
    // 第三步，用新的字符串替换原来的部分
    int tempSize = temp.size(),endSize=this->registers.size()-next;
    int newSize = this->registers.size()+tempSize-(next-p);
    this->registers.resize(newSize,0); // 字符串变长
    for(int i=newSize-1;i>newSize-endSize-1;--i)
    {
        this->registers[i]=this->registers[i-(tempSize-(next-p))];
    }
    for(int i=0;i<tempSize;++i)
    {
        this->registers[i+p]=temp[i];
    }
    // 第四步，优化序列，合并
    hllUpdateRegisters(this->registers,p);
    this->hllder.hllInvalidateCache();// 设置缓存值位无效
    return 1;

}
void HyperLogLog::hllUpdateRegisters(std::vector<uint8_t> &regs,size_t start)
{
    int size=regs.size();
    if(start >= size) return;
    int scanlen=5;
    while(start<size && scanlen--)
    {
        if(hllSparseIsZero(regs[start]))
            ++start;
        else if(hllSparseIsXzero(regs[start]))
            start+=2;
        else 
        {
            if(start+1<size && hllSparseIsVal(regs[start+1]))
            {
                int val1=hllSparseValValue(start);
                int val2=hllSparseValValue(start+1);
                if(val1 == val2)
                {
                    int len=hllSparseValLen(start) + hllSparseValLen(start+1);
                    if(len < HLL_SPARSE_VAL_MAX_LEN)
                    {
                        hllSparseValSet(this->registers,start,val1,len);
                        auto it=this->registers.begin()+start; // start+1
                        this->registers.erase(it);
                        --size;
                    }
                }
            }
            ++start;
        }
    }
}

int HyperLogLog::hllSparseAdd(std::vector<uint8_t> &regs, std::string &elem)
{
    size_t index;
    int count = static_cast<uint8_t>(hllPatLen(elem,index));
    return hllSparseSet(index,count);
}

bool HyperLogLog::hllSparseRegHisto()
{
    std::vector<int> temp(64,0);
    std::swap(this->reghisto,temp);
    int idx=0,runlen,regval;
    int start=0,end=this->registers.size();
    while(start < end)
    {
        if(hllSparseIsZero(this->registers[start]))
        {
            runlen=hllSparseZeroLen(start);
            idx += runlen;
            this->reghisto[0] += runlen;
            ++start;
        }
        else if(hllSparseIsXzero(this->registers[start]))
        {
            runlen=hllSparseXzeroLen(start);
            idx += runlen;
            this->reghisto[0] += runlen;
            start += 2;
        }
        else // VAL
        {
            runlen=hllSparseValLen(start);
            idx += runlen;
            this->reghisto[hllSparseValValue(start)] += runlen;
            ++start;
        }
    }
    if(idx != HLL_REGISTERS) return false;
    return true;
}

inline void HyperLogLog::hllRawRegHisto()
{
    for(int i=0;i<HLL_REGISTERS;++i)
    {
        ++(this->reghisto[this->registers[i]]);
    }

}

/* help function */
double HyperLogLog::hllSigma(double x)
{
    if (x == 1.) return INFINITY;
    double zPrime;
    double y = 1;
    double z = x;
    do {
        x *= x;
        zPrime = z;
        z += x * y;
        y += y;
    } while(zPrime != z);
    return z;
}

double HyperLogLog::hllTau(double x) {
    if (x == 0. || x == 1.) return 0.;
    double zPrime;
    double y = 1.0;
    double z = 1 - x;
    do {
        x = sqrt(x);
        zPrime = z;
        y *= 0.5;
        z -= pow(1 - x, 2)*y;
    } while(zPrime != z);
    return z / 3;
}


uint64_t HyperLogLog::hllCount(int &invalid)
{
    double m = HLL_REGISTERS;
    /* 根据不同编码方式计算reghisto */
    if(this->hllder.encoding == HLL_DENSE)
    {
        hllDenseRegHisto();
    }
    else if(this->hllder.encoding == HLL_SPARSE)
    {
        invalid = hllSparseRegHisto();
    }
    else if(this->hllder.encoding == HLL_RAW)
    {
        hllRawRegHisto();
    }
    else assert("error encoding!\n");

    double z = m * hllTau((m-this->reghisto[HLL_Q+1])/(double)m);
    for (int j = HLL_Q; j >= 1; --j) 
    {
        z += this->reghisto[j];
        z *= 0.5;
    }
    z += m * hllSigma(this->reghisto[0]/(double)m);
    double E = llroundl(HLL_ALPHA_INF*m*m/z);
    return static_cast<uint64_t>(E);
}

int HyperLogLog::hllAdd(std::string &elem)
{
    if(this->hllder.encoding == HLL_DENSE)
        return hllDenseAdd(this->registers,elem);
    if(this->hllder.encoding == HLL_SPARSE)
        return hllSparseAdd(this->registers,elem);
    return -1;
}

// 将reg与this->registers合并,reg必须是dense模式
int HyperLogLog::hllMerge(std::vector<uint8_t> &reg)
{
    if(this->hllder.encoding == HLL_DENSE)
    {
        uint8_t val;
        for(int i=0;i<HLL_REGISTERS;++i)
        {
            val=hllDenseGetRegister(this->registers,i);
            if(val<reg[i]) hllDenseSetRegister(this->registers,i,reg[i]); 
        }
    }
    else if(this->hllder.encoding == HLL_SPARSE)
    {
        hllSparseToDense();
        hllMerge(reg);
    }
    else assert("merge error!\n");
    return 1;
}


#endif // REDIS_LEARN_HYPERLOGLOG