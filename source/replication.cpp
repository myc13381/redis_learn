#include "replication.h"

// =====================master=============================
// 主机连接从机 主机相当于是客户端
bool connectToSlave(ServerConfig &config)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        errorHandling("socket get error!");
    sockaddr_in slave_adr;
    memset(&slave_adr, 0, sizeof(slave_adr));
    slave_adr.sin_family = AF_INET;
    slave_adr.sin_addr.s_addr = inet_addr(config.slave_IP.c_str());
    slave_adr.sin_port = htons(config.slave_port);

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (connect(sock, reinterpret_cast<sockaddr *>(&slave_adr), sizeof(slave_adr)) == -1)
    {
        errorHandling("connect error!");
        // return false;
    }
    else
    {
        config.slave_socket_fd = sock;
        debugMessage("master connect to slave success!");
        return true;
    }
    return true;
}

// 主机断开从机
void disconnectToSlave(ServerConfig &config)
{
    close(config.slave_socket_fd);
}

// 判断从机是否正常连接，发送心跳包
bool slaveIsConnected(ServerConfig &config)
{
    write(config.slave_socket_fd, PING.c_str(), PING.length());
    constexpr int buff_size = 64;
    char buff[buff_size];
    int str_len = read(config.slave_socket_fd, buff, buff_size);
    buff[str_len] = '\0';
    std::string str(buff);
    if (str == PONG)
    {
        debugMessage("slave PONG seccuss!");
        return true;
    }
    else
    {
        debugMessage("slave PONG error!");
        return false;
    }
}

// 和从机握手
ReplConnectionPack shakeHandWithSlave(ServerConfig &config)
{
    constexpr int buffsize = 128;
    int len = sizeof(ReplConnectionPack);
    config.conn.status = REPL_STATE_CHECK;
    sendToSlave(config);
    ReplConnectionPack retpack;
    int readLen = 0;
    write(config.slave_socket_fd, reinterpret_cast<void *>(&(config.conn)), len);
    readLen = read(config.slave_socket_fd, reinterpret_cast<void *>(&retpack), sizeof(ReplConnectionPack));
    if (readLen != len || retpack.status != REPL_STATE_CHECK)
    {
        showMesage("slave disconnect!!!");
        retpack.status = REPL_STATE_NULL; // 设置为无效值
        return retpack;
    }
    return retpack;
}

// 向 slave 发送包
void sendToSlave(ServerConfig &config)
{
    write(config.slave_socket_fd, reinterpret_cast<const void *>(&(config.conn)), sizeof(ReplConnectionPack));
}

// 和从机同步
void syncWithSlave(DataBase &db)
{
    ReplConnectionPack slaveConn = shakeHandWithSlave(db.config);
    if (db.config.conn.offset - slaveConn.offset == 0)
    { // 无需同步
        return;
    }
    else  // 需要同步
    {
        // 生成最新 文件
        db.db.dump_file(db.config.dumpDir);
        if (slaveConn.offset == 0 || slaveConn.offset)
        { // 全量同步
            db.config.conn.status = REPL_STATE_FULLREPL;
            sendToSlave(db.config);
            sendFile(db.config, db.config.dumpDir, 0); // 内部更新offset
        }
        else
        { // 增量同步 我们假设每次发送都是若干个完整的指令
            db.config.conn.status = REPL_STATE_INCRREPL;
            sendToSlave(db.config);
            size_t sendLen = db.cmdBinaryBuff.getSize() - slaveConn.offset;
            write(db.config.slave_socket_fd, reinterpret_cast<void *>(&sendLen), sizeof(size_t));
            const char *ch = db.cmdBinaryBuff.getbuff() + (slaveConn.offset - db.cmdBinaryBuff.getStart());
            write(db.config.slave_socket_fd, const_cast<char *>(ch), sendLen);
            db.config.conn.offset+=sendLen;
        }
    }
}

// 发送文件
void sendFile(ServerConfig &config, std::string fileName, size_t offset)
{
    constexpr size_t BUFFSIZE = 8192;
    std::ifstream file(fileName, std::ios::binary);

    char buff[BUFFSIZE];
    std::vector<char> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t fileLen = file_data.size() - offset;
    // 发送文件长度
    write(config.slave_socket_fd, reinterpret_cast<void *>(&(fileLen)), sizeof(size_t));
    // 发送文件内容
    for (int i = 0; i < fileLen; i += BUFFSIZE) // 发送文件
    {
        size_t byte_send = std::min(BUFFSIZE, file_data.size() - i);
        memcpy(buff, file_data.data() + i + offset, byte_send);
        write(config.slave_socket_fd, reinterpret_cast<void *>(buff), byte_send);
    }
    if (offset == 0)
        config.conn.offset = fileLen; // 发送了全量包
    else
        config.conn.offset += fileLen; // 发送增量包
    file.close();
}

//======================slave=============================
// 从机等待主机连接
bool connectToMaster(ServerConfig &config)
{
    int slave_sock = socket(PF_INET, SOCK_STREAM, 0);
    int master_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (slave_sock == -1 || master_sock == -1)
    {
        errorHandling("socket get error!");
    }
    sockaddr_in slave_adr, master_adr;
    memset(&slave_adr, 0, sizeof(slave_adr));
    slave_adr.sin_family = AF_INET;
    slave_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    slave_adr.sin_port = htons(config.slave_port);

    int optval = 1;
    setsockopt(slave_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(slave_sock, reinterpret_cast<sockaddr *>(&slave_adr), sizeof(slave_adr)) == -1)
    {
        errorHandling("bind error!");
        return false;
    }

    if (listen(slave_sock, 1) == -1)
    {
        errorHandling("listen error!");
        return false;
    }

    socklen_t master_addr_size = sizeof(master_adr);
    master_sock = accept(slave_sock, reinterpret_cast<sockaddr *>(&master_adr), &master_addr_size);
    if (master_sock == -1)
    {
        errorHandling("accept error!");
    }

    config.master_socket_fd = master_sock;
    debugMessage("slave connect to mater success!");
    return true;
}

// 从机断开连接
void disconnectoMaster(ServerConfig &config)
{
    close(config.master_socket_fd);
    close(config.slave_socket_fd);
}

// 和主机握手
ReplConnectionPack shakeHandWithMaster(ServerConfig &config)
{
    constexpr int buffsize = 128;
    int len = sizeof(ReplConnectionPack);
    ReplConnectionPack retpack;
    int readLen = read(config.master_socket_fd, reinterpret_cast<void *>(&retpack), len);
    if (readLen != len)
        errorHandling("slave shake hands with master error!");
    config.conn.status = REPL_STATE_CHECK;
    write(config.master_socket_fd, reinterpret_cast<const void *>(&(config.conn)), len);
    return retpack;
}

// 向 master 发送包
void sendToMaster(ServerConfig &config)
{
    write(config.master_socket_fd, reinterpret_cast<const void *>(&(config.conn)), sizeof(ReplConnectionPack));
}

// 和主机同步
void syncWithMaster(DataBase &db)
{
    constexpr int BUFFSIZE = 128;
    int packlen = sizeof(ReplConnectionPack);
    ReplConnectionPack retpack;
    while (1)
    {
        int readLen = read(db.config.master_socket_fd, reinterpret_cast<void *>(&retpack), packlen);
        switch (retpack.status)
        {
        case REPL_STATE_CHECK: // 心跳包
        {
            debugMessage("heart jump pack");
            db.config.conn.status = REPL_STATE_CHECK;
            sendToMaster(db.config);
            break;
        }
        case REPL_STATE_FULLREPL: // 全量复制
        {
            debugMessage("full replicatio");
            // 接收文件
            db.config.conn.offset = 0; // 全量更新，偏移值变为0
            // 接收全量文件
            recvFile(db.config, db.config.dumpDir); // 内部更新offset
            // 更新数据库
            db.db.clear();
            db.db.load_file(db.config.dumpDir);
            break;
        }
        case REPL_STATE_INCRREPL: // 增量复制
        {
            debugMessage("increase replicatio");
            // 首先接收发送数据的长度
            size_t readLen;
            read(db.config.master_socket_fd, reinterpret_cast<void *>(&readLen), sizeof(size_t));
            char *buff = new char[readLen + 1];
            read(db.config.master_socket_fd, reinterpret_cast<void *>(buff), readLen);
            char *ptr = buff;
            while (ptr != buff + readLen)
            {
                Command cmd;
                ptr += parseBinaryCmd(ptr, cmd);
                execCommand(db, cmd);
                db.config.conn.offset  += getLenOfCmd(cmd);
            }

            delete buff;
            break;
        }
        case REPL_STATE_LONG_CONNECT: // 长连接复制
        {
            debugMessage("long connecting replication");
            constexpr int BUFFSIZE = 1024;
            char buff[BUFFSIZE];
            read(db.config.master_socket_fd, reinterpret_cast<void *>(buff), BUFFSIZE);
            Command cmd;
            parseBinaryCmd(buff, cmd);
            // 接收到一条指令，直接执行
            execCommand(db, cmd);
            db.config.conn.offset  += getLenOfCmd(cmd);
            break;
        }
        case REPL_STATE_NULL:
        {
            break;
        }
        }
    }
}

// 接收文件
void recvFile(ServerConfig &config, std::string fileName)
{
    constexpr size_t BUFFSIZE = 8192;
    size_t fileLen = 0;
    int readLen = read(config.master_socket_fd, reinterpret_cast<void *>(&fileLen), sizeof(size_t));
    if (readLen != sizeof(size_t))
        debugMessage("revc file len error!");
    std::ofstream file(fileName, std::ios::binary);
    char *buff = new char[fileLen + 1];
    read(config.master_socket_fd, static_cast<void *>(buff), fileLen);
    file.write(buff, fileLen);
    config.conn.offset += fileLen;
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