#include "common.h"

CmdBuff::CmdBuff(int buffsize):_start(0), _end(0), _size(0), _capacity(buffsize), v(std::vector<Command>(buffsize))
{}
void CmdBuff::push_back(Command &cmd)
{
    if(_size <_capacity)
    {
        v[_end]=cmd;
        ++_size;
    }
    else 
    {
        _start=(_start+1)%_capacity;
        v[_end]=cmd;
    }
    _end = (_end+1)%_capacity;
}
Command CmdBuff::pop_front()
{
    --_size;
    Command ret = v[_start];
    _start = (_start+1)%_capacity;
    return ret;
}
    
int CmdBuff::getStart()
{
    return _start;
}
int CmdBuff::getEnd()
{
    return _end;
}
int CmdBuff::size()
{
    return _size;
}
int CmdBuff::capacity()
{
    return _capacity;
}

Command& CmdBuff::at(size_t index)
{
    assert(index < _capacity);
    return v[index];
}

Command& CmdBuff::front()
{
    return v[_start];
}

void CmdBuff::clear()
{
    _size = 0;
    _start = _end = 0;
}

// 在缓冲区尾部写入一个cmd
// 缓冲区默认大小为 REPL_COPY_BUFF 如过剩余的空间不够添加一个命令，则将原来缓冲区的一半缓存删除
void CmdBinaryBuff::writeCmdBack(Command &cmd)
{
    // 首先计算 cmd 的长度
    size_t keyLen = cmd.key.length() + 1; // 加1包括 '\0'
    size_t valueLen = cmd.value.length() + 1;
    size_t totalLen = getLenOfCmd(cmd);
    if(capacity-size < totalLen)
    {   // 需要删除前面一半空间
        char *newbuff = new char[REPL_COPY_BUFF];
        memset(newbuff,0,capacity);
        memcpy(newbuff, buff + capacity/2, capacity-capacity/2);
        delete buff;
        buff = newbuff;
        newbuff = nullptr;
        start_index += capacity/2;
        cur_index = capacity - capacity/2;
        size = capacity-capacity/2;
    }
    // 将指令写入缓冲区
    // memset(buff+cur_index, 0, totalLen); // 初始化空间
    *(CMD_FLAG*)(buff+cur_index) = cmd.cmdFlag;
    *(size_t *)(buff + sizeof(CMD_FLAG)) = keyLen;
    memcpy(buff +  sizeof(CMD_FLAG) + sizeof(size_t), cmd.key.c_str(), keyLen - 1); 
    *(size_t *)(buff + sizeof(CMD_FLAG) + sizeof(size_t) + keyLen) =  valueLen;
    memcpy(buff  + sizeof(CMD_FLAG) + 2 * sizeof(size_t) + keyLen, cmd.value.c_str(), valueLen - 1);
    cur_index += totalLen;
    size+=totalLen;
}

void execCommand(DataBase &db, Command &cmd)
{
    switch (cmd.cmdFlag)
    {
        case CMD_SET :
        db.db.insert_element(cmd.key,cmd.value);
        db.cmdbuff.push_back(cmd);
        break;

        case CMD_GET :
        db.db.search_element(cmd.key);
        break;

        default:
            std::cout<<"unknow cmd !\n";
        break;
    }
}

// 计算一个cmd转换为发送格式的长度
inline size_t getLenOfCmd(Command &cmd)
{
    size_t keyLen = cmd.key.length();
    size_t valueLen = cmd.value.length();
    size_t totalLen = sizeof(CMD_FLAG) + sizeof(size_t) * 2 + keyLen + valueLen + 2;
    return totalLen;
}


void errorHandling(std::string message)
{
    std::cerr<<message<<'\n';
    exit(1);
}

void debugMessage(std::string message)
{
    std::cout<<message<<'\n';
    return;
}

void showMesage(std::string message)
{
    std::cout<<message<<'\n';
    return;
}
