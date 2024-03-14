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

int CmdBuff::clear()
{
    _size = 0;
    _start = _end = 0;
}

void execCommand(DataBase &db, Command cmd)
{
    switch (cmd.cmdFlag)
    {
        case CMD_SET :
        db.db.insert_element(cmd.key,cmd.value);
        db.cmdbuff.push_back(cmd);
        db.config.conn.offset++; // 主从偏移值
        break;

        case CMD_GET :
        db.db.search_element(cmd.key);
        break;

        default:
            std::cout<<"unknow cmd !\n";
        break;
    }
}
