#ifndef REDIS_LEARN_COMMON
#define REDIS_LEARN_COMMON
#include <vector>
#include <string>
#include <queue>
#include <cassert>
#include <iostream>
#include <fstream>
#include <thread>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <atomic>
#include <unordered_set>
#include "skiplist.h"
#include "dict.h"
#include "threadsafe_structures.h"

#define DEFAULT_SERVER_PORT 9000

typedef Dict DB; // 数据库类型
typedef HashNode DBNode; // 数据库节点

constexpr size_t REPL_BUFF_LEN = 128; // CmdBuff 缓存长度
constexpr size_t REPL_COPY_BUFF = 1024;// 主从复制缓冲区长度，unit：byte
constexpr size_t AOF_BUFF_LEN = 128; // aof_buff 缓冲长度

enum ReplStatus {
    REPL_STATE_NONE = 0, // 初始状态
    REPL_STATE_CONNECT, // 初始化完成状态
    REPL_STATE_CONNECTING, // master 请求连接 slave
    REPL_STATE_CHECK, // 发送心跳包
    REPL_STATE_FULLREPL, // 全量复制
    REPL_STATE_INCRREPL, // 增量复制
    REPL_STATE_LONG_CONNECT, // 长连接传输
    REPL_STATE_ACK, // 回应
    REPL_STATE_NULL // 无效状态
};

// AOF 文件名相关定义
const std::string BASE_AOF_FILE_NAME = "base_aof.txt";
const std::string INCR_AOF_FILE_NAME = "incr_aof.txt";
const std::string TEMP_BASE_AOF_FILE_NAME = "temp_base_aof.txt";
const std::string TEMP_INCR_AOF_FILE_NAME = "temp_incr_aof.txt";


struct ReplConnectionPack
{
    size_t offset; // 主机发送文件的字节数或者是从机接收的文件字节数
    ReplStatus status;
    ReplConnectionPack():offset(0),status(REPL_STATE_NONE) {}
    ReplConnectionPack (const ReplConnectionPack &repl)
    {
        this->offset = repl.offset;
        this->status = repl.status;
    }
};

// 服务器 配置信息
struct ServerConfig
{
    //std::string serverID;
    bool isSlave;  // 自己是不是从机
    std::string master_IP, slave_IP; // 主机/从机的IP
    uint64_t master_port, slave_port; // 主机/从机的端口号
    int master_socket_fd,slave_socket_fd; // 主机从机套接字文件描述符
    ReplConnectionPack conn; // 握手发送的包

    std::string dumpDir; // 持久化文件的路径
};


// 命令编号
enum CMD_FLAG {
    CMD_SET = 0,
    CMD_GET,
    CMD_BGSAVE,
    CMD_SYNC,
    CMD_AOF_REWRIIE,
    CMD_SHUTDOWN
};

// 命令结构体
struct Command
{
    CMD_FLAG cmdFlag;
    std::string key;
    std::string value;
};

class CmdBuff
{
public:
    CmdBuff(int buffsize = 128);
    void push_back(Command &cmd);
    Command pop_front();
    int getStart();
    int getEnd();
    int size();
    int capacity();
    void clear();
    bool empty() { return _size == _capacity;}
    Command& at(size_t index);
    Command& front();

private:
    std::vector<Command> v;
    int _start;
    int _end;
    int _size;
    int _capacity;
};

// 复制缓冲区
// cmmond 命令格式 {CMD_FLAG}{keyLen}{key}{valueLen}{value}
class CmdBinaryBuff
{
public:
    CmdBinaryBuff():start_index(0),cur_index(0),size(0),capacity(REPL_COPY_BUFF)
    {
        buff = new char[REPL_COPY_BUFF];
        memset(buff,0,capacity);
    }
    // 在缓冲区尾部写入一个cmd
    void writeCmdBack(Command &cmd);
    size_t getStart() { return start_index; }
    const char *getbuff() { return buff; }
    size_t getSize() { return size; }
private:
    size_t start_index;
    size_t cur_index;
    size_t size;
    size_t capacity;
    char *buff;

};

class Server
{
public:
    DB db;
    ServerConfig config;

    // 主从复制命令缓存
    CmdBuff cmdbuff;
    CmdBinaryBuff cmdBinaryBuff;
    // incr_aof.txt 文件写入流
    std::ofstream incrAofStream;
    // aof_buff
    CmdBuff aof_buff;

    // 服务器运行的频率，默认10
    int hz;

    // 时间循环事件的处理次数
    int cronloops;

    // 停止服务器
    bool serverStop;

    // IO线程的数量
    size_t IOThreadNum;

    // 正在处理的 IO fd
    threadsafe_unordered_set<int> fdSet;
public:
    // 构造函数
    Server():db(6),cmdbuff(REPL_BUFF_LEN),incrAofStream(),aof_buff(AOF_BUFF_LEN),hz(10),cronloops(0),serverStop(false),IOThreadNum(1)
    {
        std::string fileName = "../"+INCR_AOF_FILE_NAME;
        incrAofStream.open(fileName,std::ios::trunc);
    }
    Server(const Server& db)
    {
        this->db = db.db;
        this->hz = db.hz;
        this->IOThreadNum = db.IOThreadNum;
        this->serverStop = false;
        this->cronloops = 0;
    }

    ~Server()
    {
        closeServer();
        this->incrAofStream.close();
    }
    // 初始化数据库服务器
    void ServerInit();

    // 服务器关闭释放资源
    void closeServer();

    void setIOThreadNum(size_t num)
    {
        this->IOThreadNum = num;
    }
};


// ====================执行相关命令================
std::string execCommand(Server &server, Command &cmd);

// 计算一个cmd转换为发送格式的长度
inline size_t getLenOfCmd(Command &cmd)
{
    size_t keyLen = cmd.key.length();
    size_t valueLen = cmd.value.length();
    size_t totalLen = sizeof(CMD_FLAG) + sizeof(size_t) * 2 + keyLen + valueLen + 2;
    return totalLen;
}

size_t parseBinaryCmd(const char *buff, Command &cmd); // 解析一个cmd，并返回，假设一定能解析成功

// 显示命令信息
void showCommand(const Command &cmd);


// ====================调试相关====================

void errorHandling(std::string message);

void debugMessage(std::string message);

void showMesage(std::string message);



// ============================AOF 相关实现========================
// Multi Part AOF 设计

// AOF_INRC AOF 增长文件
// AOF_BASE AOF 基础文件

// 如何重写AOF文件
/* This is how rewriting of the append only file in background works:
 *
 * 1) The user calls BGREWRITEAOF                                                       用户使用指令 BGREWRITEAOF
 * 
 * 2) Redis calls this function, that forks():                                          使用fork函数创建子进程，
 *    2a) the child rewrite the append only file in a temp file.                        子进程来重写AOF文件
 *    2b) the parent open a new INCR AOF file to continue writing.                      父进程继续将当前新输入的命令写入INRC AOF文件中
 * 3) When the child finished '2a' exists.
 * 4) The parent will trap the exit code, if it's OK, it will:
 *    4a) get a new BASE file name and mark the previous (if we have) as the HISTORY type 将重写的AOF文件作为新的BASE AOF文件
 *    4b) rename(2) the temp file in new BASE file name
 *    4c) mark the rewritten INCR AOFs as history type
 *    4d) persist AOF manifest file                                                     处理 AOF manifest 清单文件
 *    4e) Delete the history files use bio                                              删除历史文件
 */

// AOF 文件的格式是：
// CMD_FLAG\tKey\tValue\n
// 将 aof_buff 中de命令写入到 ofs 文件中，一般是 INCR_AOF
void writeInrcAofFile(CmdBuff &aof_buff, std::ofstream &ofs);

// 重写 BASE_AOF 文件 重写到 targetFile 文件中，做法是遍历数据库，然后依次写入文件
void reWriteBaseAofFile(Server server, std::string targetFile);

// 重写 AOF 的整个流程
void AOFRW(Server &server);




// =======================主从复制相关======================

// 主从复制
// 第一步配置数据，数据库启动后获得主/从 的IP，port
// 如果是主机，第一次主动连接从机，并将本地文件发送给从机
// 如果是从机，则不断监听主机发送的请求
const std::string PING = "PING";
const std::string PONG = "PONG";

// =====================master=============================
bool connectToSlave(ServerConfig &config); // 主机连接从机

void disconnectToSlave(ServerConfig &config); // 主机断开从机

bool slaveIsConnected(ServerConfig &config); // 判断从机是否正常连接，发送心跳包

ReplConnectionPack shakeHandWithSlave(ServerConfig &config); // 和从机握手，用于检测连接是否正常


void sendToSlave(ServerConfig &config); // 向 slave 发送包

void syncWithSlave(Server &dserver); // 和从机同步

void sendFile(ServerConfig &config, std::string fileName, size_t offset); // 发送文件


//======================slave=============================
bool connectToMaster(ServerConfig &config); // 从机等待主机连接

void disconnectoMaster(ServerConfig &config); // 从机断开连接

ReplConnectionPack shakeHandWithMaster(ServerConfig &config); // 和主机握手

void sendToMaster(ServerConfig &config); // 向 master 发送包

void syncWithMaster(Server &db); // 和主机同步

void recvFile(ServerConfig &config, std::string fileName); // 接收文件







// ====================================时间事件处理相关====================================================
#define run_with_period(_ms_) if (((_ms_) <= 1000/server.hz) || !(server.cronloops%((_ms_)/(1000/server.hz))))
// server 时间事件函数
void serverCron(Server &server);

#endif // REDIS_LEARN_COMMON