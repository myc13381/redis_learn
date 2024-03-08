#ifndef REDIS_LEARN_LISTPACK
#define REDIS_LEARN_LISTPACK

#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <climits>
#include <malloc.h>

#define LP_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */

/* lpInsert() where argument possible values: */
#define LP_BEFORE 0
#define LP_AFTER 1
#define LP_REPLACE 2


struct listpackEntry
{
    unsigned char *sval;
    size_t slen;
    long long lval;
    listpackEntry():sval(nullptr),slen(0),lval(0) {}
};

#define LP_HDR_SIZE 6       /* 32 bit total len + 16 bit number of elements. */
#define LP_HDR_NUMELE_UNKNOWN UINT16_MAX
#define LP_MAX_INT_ENCODING_LEN 9
#define LP_MAX_BACKLEN_SIZE 5
#define LP_ENCODING_INT 0
#define LP_ENCODING_STRING 1

/* Encoding 相关宏 */
#define LP_ENCODING_7BIT_UINT 0
#define LP_ENCODING_7BIT_UINT_MASK 0x80 /* 10000000*/
#define LP_ENCODING_IS_7BIT_UINT(byte) (((byte)&LP_ENCODING_7BIT_UINT_MASK)==LP_ENCODING_7BIT_UINT)
#define LP_ENCODING_7BIT_UINT_ENTRY_SIZE 2

#define LP_ENCODING_6BIT_STR 0x80
#define LP_ENCODING_6BIT_STR_MASK 0xC0 /* 11000000 */
#define LP_ENCODING_IS_6BIT_STR(byte) (((byte)&LP_ENCODING_6BIT_STR_MASK)==LP_ENCODING_6BIT_STR)

#define LP_ENCODING_13BIT_INT 0xC0
#define LP_ENCODING_13BIT_INT_MASK 0xE0 /* 11100000*/
#define LP_ENCODING_IS_13BIT_INT(byte) (((byte)&LP_ENCODING_13BIT_INT_MASK)==LP_ENCODING_13BIT_INT)
#define LP_ENCODING_13BIT_INT_ENTRY_SIZE 3

#define LP_ENCODING_12BIT_STR 0xE0
#define LP_ENCODING_12BIT_STR_MASK 0xF0
#define LP_ENCODING_IS_12BIT_STR(byte) (((byte)&LP_ENCODING_12BIT_STR_MASK)==LP_ENCODING_12BIT_STR)

#define LP_ENCODING_16BIT_INT 0xF1
#define LP_ENCODING_16BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_16BIT_INT(byte) (((byte)&LP_ENCODING_16BIT_INT_MASK)==LP_ENCODING_16BIT_INT)
#define LP_ENCODING_16BIT_INT_ENTRY_SIZE 4

#define LP_ENCODING_24BIT_INT 0xF2
#define LP_ENCODING_24BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_24BIT_INT(byte) (((byte)&LP_ENCODING_24BIT_INT_MASK)==LP_ENCODING_24BIT_INT)
#define LP_ENCODING_24BIT_INT_ENTRY_SIZE 5

#define LP_ENCODING_32BIT_INT 0xF3
#define LP_ENCODING_32BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_32BIT_INT(byte) (((byte)&LP_ENCODING_32BIT_INT_MASK)==LP_ENCODING_32BIT_INT)
#define LP_ENCODING_32BIT_INT_ENTRY_SIZE 6

#define LP_ENCODING_64BIT_INT 0xF4
#define LP_ENCODING_64BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_64BIT_INT(byte) (((byte)&LP_ENCODING_64BIT_INT_MASK)==LP_ENCODING_64BIT_INT)
#define LP_ENCODING_64BIT_INT_ENTRY_SIZE 10

#define LP_ENCODING_32BIT_STR 0xF0
#define LP_ENCODING_32BIT_STR_MASK 0xFF
#define LP_ENCODING_IS_32BIT_STR(byte) (((byte)&LP_ENCODING_32BIT_STR_MASK)==LP_ENCODING_32BIT_STR)

#define LP_EOF 0xFF /* 终止位标志 */

/* X位长度的字符串的实际长度 */
#define LP_ENCODING_6BIT_STR_LEN(p) ((p)[0] & 0x3F)
#define LP_ENCODING_12BIT_STR_LEN(p) ((((p)[0] & 0xF) << 8) | (p)[1])
#define LP_ENCODING_32BIT_STR_LEN(p) (((uint32_t)(p)[1]<<0) | \
                                      ((uint32_t)(p)[2]<<8) | \
                                      ((uint32_t)(p)[3]<<16) | \
                                      ((uint32_t)(p)[4]<<24))

/* listpack由4部分组成，Total Byte：4字节，Num Elem:2字节，End:1字节结束标志，Entry具体的每个元素 */
/* 计算listpack的Total Byte */
#define lpGetTotalBytes(p)           (((uint32_t)(p)[0]<<0) | \
                                      ((uint32_t)(p)[1]<<8) | \
                                      ((uint32_t)(p)[2]<<16) | \
                                      ((uint32_t)(p)[3]<<24))

/* 计算listpack的Num Elem */
#define lpGetNumElements(p)          (((uint32_t)(p)[4]<<0) | \
                                      ((uint32_t)(p)[5]<<8))

/* 设置Total Byte */
#define lpSetTotalBytes(p,v) do { \
    (p)[0] = (v)&0xff; \
    (p)[1] = ((v)>>8)&0xff; \
    (p)[2] = ((v)>>16)&0xff; \
    (p)[3] = ((v)>>24)&0xff; \
} while(0)

#define lpSetNumElements(p,v) do { \
    (p)[4] = (v)&0xff; \
    (p)[5] = ((v)>>8)&0xff; \
} while(0)

/* 判断合法性 */
/* Validates that 'p' is not outside the listpack.
 * All function that return a pointer to an element in the listpack will assert
 * that this element is valid, so it can be freely used.
 * Generally functions such lpNext and lpDelete assume the input pointer is
 * already validated (since it's the return value of another function). */
#define ASSERT_INTEGRITY(lp, p) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p) < (lp)+lpGetTotalBytes((lp))); \
} while (0)

/* Similar to the above, but validates the entire element length rather than just
 * it's pointer. */
#define ASSERT_INTEGRITY_LEN(lp, p, len) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p)+(len) < (lp)+lpGetTotalBytes((lp))); \
} while (0)

/* Validate that the entry doesn't reach outside the listpack allocation. */
static inline void lpAssertValidEntry(unsigned char* lp, size_t lpbytes, unsigned char *p);

/* listpack 不要超过1GB */
#define LISTPACK_MAX_SAFETY_SIZE (1<<30)


class ListPack 
{
public:
    
    ListPack(size_t capacity)
    {
        this->lp=new unsigned char[capacity > LP_HDR_SIZE+1 ? capacity : LP_HDR_SIZE+1];
        lpSetTotalBytes(this->lp,LP_HDR_SIZE+1);
        lpSetNumElements(this->lp,0);
        this->lp[LP_HDR_SIZE]=LP_EOF;
    }
    ListPack() {ListPack(0);}
    ~ListPack()
    {
        if(lp!=nullptr) delete[] lp;
    }
    // 判断添加数量是否安全,可以添加返回1,否则返回0
    int lpSafeToAdd(size_t add);

    /* 将一个整数字符串转换为一个64位的整数成功返回1否则返回0 一个比较独立的函数 */
    static int lpStringToInt64(const char *s, unsigned long slen, int64_t *value);

    /* 清理无用空间 */
    void lpShrinkToFit();

    /* Entry 中 Encoding中数字相关编码 */
    void lpEncodeIntegerGetType(int64_t v, unsigned char *intenc, uint64_t *enclen);

    /* 获取entry中的encoding 和 content 返回 LP_ENCODING_STRING 或者 LP_ENCODING_INT */
    int lpEncodeGetType(unsigned char *ele, size_t size, unsigned char *intenc, uint64_t *enclen);

    /* 将l以backlen的规则写入buf中 保证 buf[0] 的第1位是0 */
    unsigned long lpEncodeBacklen(unsigned char *buf, uint64_t l);

    /* 翻译 backlen */
    uint64_t ListPack::lpDecodeBacklen(unsigned char *p);

    /* 按照encoding规则 写入string和长度进入buf中 */
    void lpEncodeString(unsigned char *buf, unsigned char *s, uint32_t len);

    /* 获取encoding和content的size */
    uint32_t lpCurrentEncodedSizeUnsafe(unsigned char *p);

    /* 获取Encoding的长度 */
    uint32_t lpCurrentEncodedSizeBytes(unsigned char *p);
    /* 封装lpGettotalbytes */
    inline size_t lpBytes(unsigned char *lp) { return lpGetTotalBytes(lp);}

    /* 返回下一个entry 不安全*/
    unsigned char *lpSkip(unsigned char *p);

    /* 返回下一个entry 安全 */
    unsigned char *lpNext(unsigned char *lp, unsigned char *p);

    /* 返回前一个entry */
    unsigned char *ListPack::lpPrev(unsigned char *lp, unsigned char *p);

    /* 返回第一个entry  */
    unsigned char *lpFirst(unsigned char *lp) 
    {
        unsigned char *p = lp + LP_HDR_SIZE; /* Skip the header. */
        if (p[0] == LP_EOF) return NULL;
        lpAssertValidEntry(lp, lpBytes(lp), p);
        return p;
    }

    /* 返回最后一个entry */
    unsigned char *lpLast(unsigned char *lp) 
    {
        unsigned char *p = lp+lpGetTotalBytes(lp)-1; /* Seek EOF element. */
        return lpPrev(lp,p); /* Will return NULL if EOF is the only element. */
    }

    /* 计算listpack中元素的数量，依次遍历的方式 */
    unsigned long lpLength(unsigned char *lp);

    /* 获取p位置的entry */
    unsigned char *ListPack::lpGetWithSize(unsigned char *p, int64_t *count, unsigned char *intbuf, uint64_t *entry_size);

    /* 封装了lpGetWithSize函数 */
    unsigned char *lpGet(unsigned char *p, int64_t *count, unsigned char *intbuf) {return lpGetWithSize(p, count, intbuf, NULL);}

    /* 封装了lpGet,如果entry存放的是str，返回str的指针，参数slen存放str长度，如果entry存放数字，返回NULL，参数lval返回数字 */
    unsigned char *lpGetValue(unsigned char *p, unsigned int *slen, long long *lval);

    /* lp是listpack指针，elestr和eleint是插入的元素，size是前两者的长度，如果elestr和eleint均为null，则表示删除该元素，p是元素位置指针，where是标志LP_BEFORE, LP_AFTER
    or LP_REPLACE 在内存不足或列表包总长度超过时返回 NULL 如果 'newp' 不是 NULL，则在成功调用结束时将设置 '*newp'
    添加到刚刚添加的元素的地址，以便可以
    继续与 lpNext和 lpPrev 交互。对于删除操作 ，'newp' 是设置为下一个元素 */
    unsigned char *lpInsert(unsigned char *lp, unsigned char *elestr, unsigned char *eleint,
                        uint32_t size, unsigned char *p, int where, unsigned char **newp);
    /* 插入字符串，s为字符串原始值，编码操作在lpInsert函数内部完成 */
    unsigned char *lpInsertString(unsigned char *lp, unsigned char *s, uint32_t slen,
                                unsigned char *p, int where, unsigned char **newp)
    { return lpInsert(lp, s, NULL, slen, p, where, newp); }
    
    /* This is just a wrapper for lpInsert() to directly use a 64 bit integer instead of a string. */
    unsigned char *lpInsertInteger(unsigned char *lp, long long lval, unsigned char *p, int where, unsigned char **newp)
    {
        uint64_t enclen; /* The length of the encoded element. */
        unsigned char intenc[LP_MAX_INT_ENCODING_LEN];
        lpEncodeIntegerGetType(lval, intenc, &enclen); /* 先将数字进行编码 */
        return lpInsert(lp, NULL, intenc, enclen, p, where, newp);
    }

    /* 在尾部添加字符串元素，相关参数见lpInsert */
    unsigned char *lpAppend(unsigned char *lp, unsigned char *ele, uint32_t size)
    {
        uint64_t listpack_bytes = lpGetTotalBytes(lp);
        unsigned char *eofptr = lp + listpack_bytes - 1;
        return lpInsert(lp,ele,NULL,size,eofptr,LP_BEFORE,NULL);
    }

    /* 在尾部添加一个整型元素 */
    unsigned char *lpAppendInteger(unsigned char *lp, long long lval)
    {
        uint64_t listpack_bytes = lpGetTotalBytes(lp);
        unsigned char *eofptr = lp + listpack_bytes - 1;
        return lpInsertInteger(lp, lval, eofptr, LP_BEFORE, NULL);
    }

    /* 在头部添加一个字符串元素 */
    unsigned char *lpPrepend(unsigned char *lp, unsigned char *s, uint32_t slen)
    {
        unsigned char *p = lpFirst(lp);
        if (!p) return lpAppend(lp, s, slen);
        return lpInsert(lp, s, NULL, slen, p, LP_BEFORE, NULL);
    }

    /* 在头部添加一个整型元素 */
    unsigned char *lpPrependInteger(unsigned char *lp, long long lval)
    {
        unsigned char *p = lpFirst(lp);
        if (!p) return lpAppendInteger(lp, lval);
        return lpInsertInteger(lp, lval, p, LP_BEFORE, NULL);
    }

    /* 替换一个字符串元素*/
    unsigned char *lpReplace(unsigned char *lp, unsigned char **p, unsigned char *s, uint32_t slen)
    {
        return lpInsert(lp, s, NULL, slen, *p, LP_REPLACE, p);
    }

    /* 尾部添加一个字符串元素 */
    unsigned char *lpAppend(unsigned char *lp, unsigned char *ele, uint32_t size)
    {
        uint64_t listpack_bytes = lpGetTotalBytes(lp);
        unsigned char *eofptr = lp + listpack_bytes - 1;
        return lpInsert(lp,ele,NULL,size,eofptr,LP_BEFORE,NULL);
    }
    /* 尾部添加一个整型元素. */
    unsigned char *lpAppendInteger(unsigned char *lp, long long lval)
    {
        uint64_t listpack_bytes = lpGetTotalBytes(lp);
        unsigned char *eofptr = lp + listpack_bytes - 1;
        return lpInsertInteger(lp, lval, eofptr, LP_BEFORE, NULL);
    }

    /* 删除一个元素 */
    unsigned char *lpDelete(unsigned char *lp, unsigned char *p, unsigned char **newp)
    {
        return lpInsert(lp,NULL,NULL,0,p,LP_REPLACE,newp);
    }

    /* Delete a range of entries from the listpack start with the element pointed by 'p'.
        删除p指向元素为起始位置的num个元素 p为二级指针，最后指向删除元素之后的那个元素，返回新的lp*/
    unsigned char *lpDeleteRangeWithEntry(unsigned char *lp, unsigned char **p, unsigned long num);

private:
    unsigned char *lp;
};

int ListPack::lpSafeToAdd(size_t add)
{
    size_t size=this->lp==nullptr?lpGetTotalBytes(this->lp):0;
    if(size+add>LISTPACK_MAX_SAFETY_SIZE) return 0;
    return 1;
}

int ListPack::lpStringToInt64(const char *s, unsigned long slen, int64_t *value)
{
    const char *p = s;
    unsigned long plen = 0;
    int negative = 0;
    uint64_t v;

    /* Abort if length indicates this cannot possibly be an int */
    if (slen == 0 || slen >= 21)
        return 0;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (UINT64_MAX / 10)) /* Overflow. */
            return 0;
        v *= 10;

        if (v > (UINT64_MAX - (p[0]-'0'))) /* Overflow. */
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return 0;

    if (negative) {
        if (v > ((uint64_t)(-(INT64_MIN+1))+1)) /* Overflow. */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > INT64_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}


void ListPack::lpShrinkToFit()
{
    return;
}

void ListPack::lpEncodeIntegerGetType(int64_t v, unsigned char *intenc, uint64_t *enclen)
{
    if(v>=0 && v<=127)
    { // 使用一个字节即可保存
        intenc[0]=v;
        *enclen=1;
    }
    else if(v>=-4096 && v<=4095)
    { // [-4096 0)U[128,4095] 使用13位即可保存，共使用两个字节，其中[-4096 0) 使用[4096,8192)保存 下面类似
        if(v<0) v=static_cast<int64_t>(1<<13)+v;
        intenc[0]=(v>>8) | LP_ENCODING_13BIT_INT;
        intenc[1]=v & 0xff;
        *enclen=2;
    }
    else if(v>=-32768 && v<=32767)
    { // 16bit
        if(v<0) v=static_cast<int64_t>(1<<16)+v;
        intenc[0]=LP_ENCODING_16BIT_INT;
        intenc[1]=v & 0xff;
        intenc[2]=v>>8;
        *enclen=3;
    }
    else if(v>=8388608 && v<=8388607)
    { // 24bit
        if(v<0) v=static_cast<int64_t>(1<<24)+v;
        intenc[0]=LP_ENCODING_24BIT_INT;
        intenc[1]=v & 0xff;
        intenc[2]=(v>>8)&0xff;
        intenc[3]=v>>16;
        *enclen=4;
    }
    else if(v>=-2147483648 && v<=2147483647)
    { // 32bit
        if(v<0) v=static_cast<int64_t>(1<<32)+v;
        intenc[0]=LP_ENCODING_32BIT_INT;
        intenc[1]=v & 0xff;
        intenc[2]=(v>>8)&0xff;
        intenc[3]=(v>>16)&0xff;
        intenc[4]=v && 0xff;
        *enclen=5;
    }
    else  // 64bit
    {
        uint64_t uv=static_cast<uint64_t>(v);
        intenc[0]=LP_ENCODING_64BIT_INT;
        intenc[1]=v & 0xff;
        intenc[2]=(v>>8)&0xff;
        intenc[4]=(v>>16)&0xff;
        intenc[5]=(v>>24)&0xff;
        intenc[6]=(v>>32)&0xff;
        intenc[7]=(v>>48)&0xff;
        intenc[8]=v>>56;
        *enclen=9;
    }
}

int ListPack::lpEncodeGetType(unsigned char *ele, size_t size, unsigned char *intenc, uint64_t *enclen)
{
    int64_t v;
    if(lpStringToInt64(reinterpret_cast<const char*>(ele),size,&v))
    {
        lpEncodeIntegerGetType(v,intenc,enclen);
        return LP_ENCODING_INT;
    }
    else
    {
        if(size<64) *enclen=size+1;
        else if(size<4096) *enclen=size+2;
        else *enclen = static_cast<uint64_t>(size)+5;
        return LP_ENCODING_STRING;
    }
}

unsigned long ListPack::lpEncodeBacklen(unsigned char *buf, uint64_t l)
{
    int ret;
    // |128 是为了让最高位变成1
    if(l<=127)
    {
        if(buf) buf[0]=l;
        ret=1;
    }
    else if(l<16383)
    {
        if(buf)
        {
            buf[0]=l>>7;
            buf[1]=(l&127)|128;
        }
        ret=2;
    }
    else if(l<2097151)
    {
        if(buf)
        {
            buf[0]=l>>14;
            buf[1]=((l>>7)&127)|128;
            buf[2]=(l&127)|128;
        }
        ret=3;
    }
    else if(l<268435455)
    {
        if(buf)
        {
            buf[0]=l>>21;
            buf[1]=((l>>14)&127)|128;
            buf[2]=((l>>7)&127)|128;
            buf[3]=(l&127)|128;
        }
        ret=4;
    }
    else
    {
        if(buf)
        {
            buf[0]=l>>28;
            buf[1]=((l>>21)&127)|128;
            buf[2]=((l>>14)&127)|128;
            buf[3]=((l>>7)&127)|128;
            buf[4]=(l&127)|128;
        }
        ret=5;
    }
    return ret;
}

/* 将 backlen 翻译出来 */
uint64_t ListPack::lpDecodeBacklen(unsigned char *p) {
    uint64_t val = 0;
    uint64_t shift = 0;
    do {
        val |= (uint64_t)(p[0] & 127) << shift;
        if (!(p[0] & 128)) break;
        shift += 7;
        p--;
        if (shift > 28) return UINT64_MAX;
    } while(1);
    return val;
}

void ListPack::lpEncodeString(unsigned char *buf, unsigned char *s, uint32_t len)
{
    if(len<64)
    {
        buf[0]=len | LP_ENCODING_6BIT_STR;
        std::memcpy(buf+1,s,len);
    }
    else if(len<4096)
    {
        buf[0]=(len>>8)|LP_ENCODING_12BIT_STR;
        buf[1]=len & 0xff;
        std::memcpy(buf+2,s,len);
    }
    else 
    {
        buf[0] = LP_ENCODING_32BIT_STR;
        buf[1] = len & 0xff;
        buf[2] = (len >> 8) & 0xff;
        buf[3] = (len >> 16) & 0xff;
        buf[4] = (len >> 24) & 0xff;
        memcpy(buf+5,s,len);
    }
}

uint32_t ListPack::lpCurrentEncodedSizeUnsafe(unsigned char *p)
{
    /* 利用前缀进行比较 */
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) return 1;
    if (LP_ENCODING_IS_6BIT_STR( p[0])) return 1+LP_ENCODING_6BIT_STR_LEN(p);
    if (LP_ENCODING_IS_13BIT_INT(p[0])) return 2;
    if (LP_ENCODING_IS_16BIT_INT(p[0])) return 3;
    if (LP_ENCODING_IS_24BIT_INT(p[0])) return 4;
    if (LP_ENCODING_IS_32BIT_INT(p[0])) return 5;
    if (LP_ENCODING_IS_64BIT_INT(p[0])) return 9;
    if (LP_ENCODING_IS_12BIT_STR(p[0])) return 2+LP_ENCODING_12BIT_STR_LEN(p);
    if (LP_ENCODING_IS_32BIT_STR(p[0])) return 5+LP_ENCODING_32BIT_STR_LEN(p);
    if (p[0] == LP_EOF) return 1;
    return 0;
}

uint32_t ListPack::lpCurrentEncodedSizeBytes(unsigned char *p) {
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) return 1;
    if (LP_ENCODING_IS_6BIT_STR(p[0])) return 1;
    if (LP_ENCODING_IS_13BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_16BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_24BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_32BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_64BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_12BIT_STR(p[0])) return 2;
    if (LP_ENCODING_IS_32BIT_STR(p[0])) return 5;
    if (p[0] == LP_EOF) return 1;
    return 0;
}

unsigned char *ListPack::lpSkip(unsigned char *p) 
{
    unsigned long entrylen = lpCurrentEncodedSizeUnsafe(p);
    entrylen += lpEncodeBacklen(NULL,entrylen);
    p += entrylen;
    return p;
}

unsigned char *ListPack::lpNext(unsigned char *lp, unsigned char *p) 
{
    assert(p);
    p = lpSkip(p);
    if (p[0] == LP_EOF) return NULL;
    lpAssertValidEntry(lp, lpBytes(lp), p);
    return p;
}


unsigned char *ListPack::lpPrev(unsigned char *lp, unsigned char *p) {
    assert(p);
    if (p-lp == LP_HDR_SIZE) return NULL;
    p--; /* Seek the first backlen byte of the last element. */
    uint64_t prevlen = lpDecodeBacklen(p);
    prevlen += lpEncodeBacklen(NULL,prevlen);
    p -= prevlen-1; /* Seek the first byte of the previous entry. */
    lpAssertValidEntry(lp, lpBytes(lp), p);
    return p;
}

    /* 计算listpack中元素的数量，依次遍历的方式 */
unsigned long ListPack::lpLength(unsigned char *lp) 
{
    uint32_t numele = lpGetNumElements(lp);
    if (numele != LP_HDR_NUMELE_UNKNOWN) return numele;

    /* Too many elements inside the listpack. We need to scan in order
     * to get the total number. */
    uint32_t count = 0;
    unsigned char *p = lpFirst(lp);
    while(p) {
        count++;
        p = lpNext(lp,p);
    }
    /* If the count is again within range of the header numele field,
     * set it. */
    if (count < LP_HDR_NUMELE_UNKNOWN) lpSetNumElements(lp,count);
    return count;
}





unsigned char *ListPack::lpGetWithSize(unsigned char *p, int64_t *count, unsigned char *intbuf, uint64_t *entry_size) {
    int64_t val;
    uint64_t uval, negstart, negmax;

    assert(p); /* assertion for valgrind (avoid NPD) */
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) {
        negstart = UINT64_MAX; /* 7 bit ints are always positive. */
        negmax = 0;
        uval = p[0] & 0x7f;
        if (entry_size) *entry_size = LP_ENCODING_7BIT_UINT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_6BIT_STR(p[0])) {
        *count = LP_ENCODING_6BIT_STR_LEN(p);
        if (entry_size) *entry_size = 1 + *count + lpEncodeBacklen(NULL, *count + 1);
        return p+1;
    } else if (LP_ENCODING_IS_13BIT_INT(p[0])) {
        uval = ((p[0]&0x1f)<<8) | p[1];
        negstart = (uint64_t)1<<12;
        negmax = 8191;
        if (entry_size) *entry_size = LP_ENCODING_13BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_16BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8;
        negstart = (uint64_t)1<<15;
        negmax = UINT16_MAX;
        if (entry_size) *entry_size = LP_ENCODING_16BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_24BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16;
        negstart = (uint64_t)1<<23;
        negmax = UINT32_MAX>>8;
        if (entry_size) *entry_size = LP_ENCODING_24BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_32BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16 |
               (uint64_t)p[4]<<24;
        negstart = (uint64_t)1<<31;
        negmax = UINT32_MAX;
        if (entry_size) *entry_size = LP_ENCODING_32BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_64BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16 |
               (uint64_t)p[4]<<24 |
               (uint64_t)p[5]<<32 |
               (uint64_t)p[6]<<40 |
               (uint64_t)p[7]<<48 |
               (uint64_t)p[8]<<56;
        negstart = (uint64_t)1<<63;
        negmax = UINT64_MAX;
        if (entry_size) *entry_size = LP_ENCODING_64BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_12BIT_STR(p[0])) {
        *count = LP_ENCODING_12BIT_STR_LEN(p);
        if (entry_size) *entry_size = 2 + *count + lpEncodeBacklen(NULL, *count + 2);
        return p+2;
    } else if (LP_ENCODING_IS_32BIT_STR(p[0])) {
        *count = LP_ENCODING_32BIT_STR_LEN(p);
        if (entry_size) *entry_size = 5 + *count + lpEncodeBacklen(NULL, *count + 5);
        return p+5;
    } else {
        uval = 12345678900000000ULL + p[0];
        negstart = UINT64_MAX;
        negmax = 0;
    }

    /* We reach this code path only for integer encodings.
     * Convert the unsigned value to the signed one using two's complement
     * rule. */
    /* 将val从整数表示转换为负数表示 */
    if (uval >= negstart) {
        /* This three steps conversion should avoid undefined behaviors
         * in the unsigned -> signed conversion. */
        uval = negmax-uval;
        val = uval;
        val = -val-1;
    } else {
        val = uval;
    }

    /* Return the string representation of the integer or the value itself
     * depending on intbuf being NULL or not. */
    if (intbuf) {
        std::string str=std::to_string(static_cast<long long>(val));
        *intbuf = *(std::to_string(static_cast<long long>(val)).c_str());
        *count = str.length();
        return intbuf;
    } else {
        *count = val;
        return NULL;
    }
}

unsigned char *ListPack::lpGetValue(unsigned char *p, unsigned int *slen, long long *lval) {
    unsigned char *vstr;
    int64_t ele_len;

    vstr = lpGet(p, &ele_len, NULL);
    if (vstr) {
        *slen = ele_len;
    } else {
        *lval = ele_len;
    }
    return vstr;
}


/* 如果是elestr则传入的实际字符串值，如果是eleint则传入的是int类型编码后的值 */
unsigned char *ListPack::lpInsert(unsigned char *lp, unsigned char *elestr, unsigned char *eleint,
                        uint32_t size, unsigned char *p, int where, unsigned char **newp)
{
    unsigned char intenc[LP_MAX_INT_ENCODING_LEN];
    unsigned char backlen[LP_MAX_BACKLEN_SIZE];

    uint64_t enclen; /* The length of the encoded element. */
    int _delete = (elestr == NULL && eleint == NULL);

    /* when deletion, it is conceptually replacing the element with a
     * zero-length element. So whatever we get passed as 'where', set
     * it to LP_REPLACE. */
    if (_delete) where = LP_REPLACE;

    /* If we need to insert after the current element, we just jump to the
     * next element (that could be the EOF one) and handle the case of
     * inserting before. So the function will actually deal with just two
     * cases: LP_BEFORE and LP_REPLACE. */
    /* 统一变更为LP_BEFORE */
    if (where == LP_AFTER) {
        p = lpSkip(p); /* 也就是说EOF之后也会有一个backlen字段，一个字节 */
        where = LP_BEFORE;
        ASSERT_INTEGRITY(lp, p);
    }

    /* Store the offset of the element 'p', so that we can obtain its
     * address again after a reallocation. */
    unsigned long poff = p-lp;

    int enctype;
    if (elestr) {
        /* Calling lpEncodeGetType() results into the encoded version of the
        * element to be stored into 'intenc' in case it is representable as
        * an integer: in that case, the function returns LP_ENCODING_INT.
        * Otherwise if LP_ENCODING_STR is returned, we'll have to call
        * lpEncodeString() to actually write the encoded string on place later.
        *
        * Whatever the returned encoding is, 'enclen' is populated with the
        * length of the encoded element. */
        enctype = lpEncodeGetType(elestr,size,intenc,&enclen);
        if (enctype == LP_ENCODING_INT) eleint = intenc;
    } else if (eleint) {
        enctype = LP_ENCODING_INT;
        enclen = size; /* 'size' is the length of the encoded integer element. eleint是已经编码的状态*/
    } else { // elestr和eleint均为null，所以删除
        enctype = -1;
        enclen = 0;
    }

    /* We need to also encode the backward-parsable length of the element
     * and append it to the end: this allows to traverse the listpack from
     * the end to the start. */
    unsigned long backlen_size = (!_delete) ? lpEncodeBacklen(backlen,enclen) : 0;/* backlen 编码 */
    uint64_t old_listpack_bytes = lpGetTotalBytes(lp); /* 之前listpack的总长度，uint：byte */
    uint32_t replaced_len  = 0;
    if (where == LP_REPLACE) {
        replaced_len = lpCurrentEncodedSizeUnsafe(p);
        replaced_len += lpEncodeBacklen(NULL,replaced_len);
        ASSERT_INTEGRITY_LEN(lp, p, replaced_len);
    }

    uint64_t new_listpack_bytes = old_listpack_bytes + enclen + backlen_size
                                  - replaced_len;
    if (new_listpack_bytes > UINT32_MAX) return NULL;

    /* We now need to reallocate in order to make space or shrink the
     * allocation (in case 'when' value is LP_REPLACE and the new element is
     * smaller). However we do that before memmoving the memory to
     * make room for the new element if the final allocation will get
     * larger, or we do it after if the final allocation will get smaller. */

    unsigned char *dst = lp + poff; /* May be updated after reallocation. */

    /* Realloc before: we need more room. */
    if (new_listpack_bytes > old_listpack_bytes &&
        new_listpack_bytes > malloc_usable_size(lp)) {
        if ((lp = static_cast<unsigned char*>(realloc(lp,new_listpack_bytes))) == NULL) return NULL;
        dst = lp + poff;
    }

    /* Setup the listpack relocating the elements to make the exact room
     * we need to store the new one. */
    if (where == LP_BEFORE) {
        memmove(dst+enclen+backlen_size,dst,old_listpack_bytes-poff);
    } else { /* LP_REPLACE. 替换*/
        memmove(dst+enclen+backlen_size,
                dst+replaced_len,
                old_listpack_bytes-poff-replaced_len);
    }

    /* Realloc after: we need to free space. */
    if (new_listpack_bytes < old_listpack_bytes) {
        if ((lp = static_cast<unsigned char*>(realloc(lp,new_listpack_bytes))) == NULL) return NULL;
        dst = lp + poff;
    }

    /* Store the entry. */
    if (newp) {
        *newp = dst;
        /* In case of deletion, set 'newp' to NULL if the next element is
         * the EOF element. */
        if (_delete && dst[0] == LP_EOF) *newp = NULL;
    }
    if (!_delete) {
        if (enctype == LP_ENCODING_INT) {
            memcpy(dst,eleint,enclen);
        } else if (elestr) {
            lpEncodeString(dst,elestr,size);
        }
        dst += enclen;
        memcpy(dst,backlen,backlen_size);
        dst += backlen_size;
    }

    /* Update header. 更新头部信息 */
    if (where != LP_REPLACE || _delete) {
        uint32_t num_elements = lpGetNumElements(lp);
        if (num_elements != LP_HDR_NUMELE_UNKNOWN) {
            if (!_delete)
                lpSetNumElements(lp,num_elements+1);
            else
                lpSetNumElements(lp,num_elements-1);
        }
    }
    lpSetTotalBytes(lp,new_listpack_bytes);
    return lp;
}

unsigned char *ListPack::lpDeleteRangeWithEntry(unsigned char *lp, unsigned char **p, unsigned long num)
{
    /* 主要是通过memmove 和 shrink_to_fit 实现的 */
    size_t bytes = lpBytes(lp);
    unsigned long deleted = 0;
    unsigned char *eofptr = lp + bytes - 1;
    unsigned char *first, *tail;
    first = tail = *p;

    if (num == 0) return lp;  /* Nothing to delete, return ASAP. */

    /* Find the next entry to the last entry that needs to be deleted.
     * lpLength may be unreliable due to corrupt data, so we cannot
     * treat 'num' as the number of elements to be deleted. */
    while (num--) {
        deleted++;
        tail = lpSkip(tail);
        if (tail[0] == LP_EOF) break;
        lpAssertValidEntry(lp, bytes, tail);
    }

    /* Store the offset of the element 'first', so that we can obtain its
     * address again after a reallocation. */
    unsigned long poff = first-lp;

    /* Move tail to the front of the listpack */
    std::memmove(first, tail, eofptr - tail + 1);
    lpSetTotalBytes(lp, bytes - (tail - first));
    uint32_t numele = lpGetNumElements(lp);
    if (numele != LP_HDR_NUMELE_UNKNOWN)
        lpSetNumElements(lp, numele-deleted);
        lpShrinkToFit();

    /* Store the entry. */
    *p = lp+poff;
    if ((*p)[0] == LP_EOF) *p = NULL;

    return lp;
}

#endif // REDIS_LEARN_LISTPACK