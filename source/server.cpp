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

// 解析一个cmd，并返回，假设一定能解析成功
size_t parseBinaryCmd(const char *buff, Command &cmd)
{
    cmd.cmdFlag = *(CMD_FLAG *)buff;
    size_t keyLen = *(size_t *)(buff + sizeof(CMD_FLAG));
    cmd.key = buff + sizeof(CMD_FLAG) + sizeof(size_t);
    size_t valueLen = *(size_t *)(buff + sizeof(CMD_FLAG) + sizeof(size_t) + keyLen);
    cmd.value = buff + sizeof(CMD_FLAG) + 2 * sizeof(size_t) + keyLen;
    return sizeof(CMD_FLAG) + 2 * sizeof(size_t) + keyLen + valueLen;
}

std::string execCommand(Server &server, Command &cmd)
{
    std::string ret;
    switch (cmd.cmdFlag)
    {
        case CMD_SET :
        {
            server.db.insert(cmd.key, cmd.value);
            server.cmdbuff.push_back(cmd);
            // 处理AOF缓存
            if(server.aof_buff.size() == server.aof_buff.capacity())
            {
                // 缓冲区已满，直接写入
                writeInrcAofFile(server.aof_buff,server.incrAofStream);
                server.aof_buff.clear();
            }
            server.aof_buff.push_back(cmd);
            ret = "ok";
            break;
        }
        case CMD_GET :
        {
            HashNode * node = server.db.find(cmd.key);
            if(node != nullptr) ret = node->getValue();
            else ret = "Not found!";
            break;
        }
        case CMD_SHUTDOWN:
        {
            server.serverStop = true;
            ret = "server is shutdown!";
            break;
        }
        default:
            std::cout<<"unknow cmd !\n";
            ret = "unknow cmd !";
        break;
    }
    return ret;
}


// 显示命令信息
void showCommand(const Command &cmd)
{
    switch (cmd.cmdFlag)
    {
        case CMD_SET :
        {
           std::cout<<"CMD_SET"<<" ";
            break;
        }
        case CMD_GET :
        {
            std::cout<<"CMD_GET"<<" ";
            break;
        }
        case CMD_SHUTDOWN :
        {
            std::cout<<"CMD_SHUTDOWN"<<" ";
            break;
        }
        default:
            std::cout<<"unknow cmd ! ";
        break;
    }
    std::cout<<"key: "<<cmd.key<<", ";
    std::cout<<"value: "<<cmd.value<<"\n";
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
        ofs<<aof_buff.at(i).cmdFlag<<' ';
        ofs<<aof_buff.at(i).key<<' ';
        ofs<<aof_buff.at(i).value<<'\n';
    }
    aof_buff.clear();
}

// 重写 BASE_AOF 文件 重写到 targetFile 文件中，做法是遍历数据库，然后依次写入文件
// 重写 AOF 文件和 rehash 不能同时发生 ，因此只需要遍历 Dict::_hashtable[0] 即可
void reWriteBaseAofFile(Server server, std::string targetFile)
{
    // if(server.db.isRehashing()) return; // 正在重哈希，不允许AOF
    std::ofstream ofs;
    ofs.open(targetFile,std::ios::trunc);
    if(ofs.is_open())
    {
        for(size_t i=0;i<server.db.getTable()[0].bucketSize();++i)
        {
            HashNode *node = server.db.getTable()[0].getBucket()[i];
            if(node == nullptr) continue;
            while(node != nullptr)
            {
                ofs<<CMD_SET<<" ";
                ofs<<node->getKey()<<" ";
                ofs<<node->getValue()<<" ";
                node = node->next();
            }
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
    // 持续尝试rehash
    server.db.rehashMilliseconds(100);

    ++server.cronloops;
}


void Server::ServerInit()
{
    // 设置端口号
    this->config.master_port = DEFAULT_SERVER_PORT;
    // Redis server 创建监听套接字
    this->config.master_socket_fd = socket(PF_INET, SOCK_STREAM, 0);

    sockaddr_in master_adr;
    memset(&master_adr, 0, sizeof(master_adr));
    master_adr.sin_family = AF_INET;
    master_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    master_adr.sin_port = htons(config.master_port);

    // 设置为socket为立即可用，便于调试
    int optval = 1;
    setsockopt(this->config.master_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // 套接字和IP，端口绑定
    if(bind(this->config.master_socket_fd, reinterpret_cast<sockaddr *>(&master_adr), sizeof(master_adr))==-1)
    {
        errorHandling("bind error!");
    }

    // 监听
    if(listen(this->config.master_socket_fd,1) == -1)
    {
        errorHandling("listen error!");
    }  
}

void Server::closeServer()
{
    close(this->config.master_socket_fd);

    // 释放客户端 fd 。。。
}