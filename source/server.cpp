#include "server.h"

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

void execCommand(Server &server, Command &cmd)
{
    switch (cmd.cmdFlag)
    {
        case CMD_SET :
        server.db.insert_element(cmd.key,cmd.value);
        server.cmdbuff.push_back(cmd);
        // 处理AOF缓存
        if(server.aof_buff.size() == server.aof_buff.capacity())
        {
            // 缓冲区已满，直接写入
            writeInrcAofFile(server.aof_buff,server.incrAofStream);
            server.aof_buff.clear();
        }
        server.aof_buff.push_back(cmd);
        break;

        case CMD_GET :
        server.db.search_element(cmd.key);
        break;

        default:
            std::cout<<"unknow cmd !\n";
        break;
    }
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

// 将 aof_buff 中de命令写入到 ofs 文件中，一般是 INCR_AOF
void writeInrcAofFile(CmdBuff &aof_buff, std::ofstream &ofs)
{
    // 将 aof_buff 中的所有命令拿出来，写入到文件中
    int n = aof_buff.size();
    for(int i=0;i<n;++i)
    {
        ofs<<aof_buff.at(i).cmdFlag<<'\t';
        ofs<<aof_buff.at(i).key<<'\t';
        ofs<<aof_buff.at(i).value<<'\n';
    }
    aof_buff.clear();
}

// 重写 BASE_AOF 文件 重写到 targetFile 文件中，做法是遍历数据库，然后依次写入文件
void reWriteBaseAofFile(Server server, std::string targetFile)
{
    std::ofstream ofs;
    ofs.open(targetFile,std::ios::trunc);
    if(ofs.is_open())
    {
        // for(int i=0;i<100;++i) ofs<<i<<' ';
        // ofs<<'\n';

        DBNode *node = const_cast<DBNode*>(server.db.getHeader())->forward[0];
        while(node != nullptr)
        {
            ofs<<CMD_SET<<'\t';
            ofs<<node->get_key()<<'\t';
            ofs<<node->get_value()<<'\n';
            node = node->forward[0];
        }
        ofs.close();
    }
    else 
    {
        debugMessage("reWriteBaseAofFile error!!!");
    }
}

// 重写 AOF 的整个流程
void AOFRW(Server &server)
{
    // 关闭之前写入 incr_aof_file 的ofstream，将原来的文件重命名，然后产生新的 incr_aof_file 文件
    if(server.incrAofStream.is_open()) server.incrAofStream.close();
    std::string oldIncrName = "../"+INCR_AOF_FILE_NAME;
    std::string tempIncrName = "../"+TEMP_INCR_AOF_FILE_NAME;
    // 文件重命名
    rename(oldIncrName.c_str(),tempIncrName.c_str());
    

    // 将之前的 base_aof_file 重命名，然后开辟子线程用来重写base_aof_file
    std::string oldBaseName = "../"+BASE_AOF_FILE_NAME;
    std::string tempBaseName = "../"+TEMP_BASE_AOF_FILE_NAME;
    rename(oldBaseName.c_str(),tempBaseName.c_str());
    std::thread childThread(reWriteBaseAofFile, server, oldBaseName);
    childThread.detach();
    // 然后产生新的 incr_aof_file 文件, ofstream 指向新的文件，父进程继续添加AOF文件
    // 删除以前的文件
    remove(tempBaseName.c_str()); 
    remove(tempIncrName.c_str());
    server.incrAofStream.open(oldIncrName,std::ios::trunc);
    
}



// server 时间事件函数
// 在redis中，该函数用来顺序调用一些其他函数处理一些后台任务
// 例如检测进程结束信号，用来关闭redis server，清除连接超时的客户端
// 还会调用 databaseCron 用来处理一些过期的键以及rehash操作
// 利用 run_with_period 宏来一定固定频率执行任务
void serverCron(Server &server)
{
    // 以一秒的频率执行AOF文件磁盘写入操作
    run_with_period(1000)
    {
        writeInrcAofFile(server.aof_buff, server.incrAofStream);
        server.aof_buff.clear();
    }

    // 每十秒发送一次 主从心跳包？

    ++server.cronloops;
}