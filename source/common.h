#ifndef REDIS_LEARN_COMMON
#define REDIS_LEARN_COMMON
#include <vector>
#include <string>
#include <queue>
#include <cassert>
#include <iostream>
#include <fstream>
#include "skiplist.h"

typedef SkipList<std::string,std::string> DB; // 数据库类型

constexpr size_t REPL_BUFF_LEN = 128; // 主从复制缓存buff长度
constexpr size_t AOF_BUFF_LEN = 128; // aof_buff 缓冲长度

enum ReplStatus {
    REPL_STATE_NONE = 0, // 初始状态
    REPL_STATE_CONNECT, // 初始化完成状态
    REPL_STATE_CONNECTING, // master 请求连接 slave
    REPL_STATE_CHECK, // 发送心跳包
    REPL_STATE_FULLREPL, // 全量复制
    REPL_STATE_INCRREPL, // 增量复制
    REPL_STATE_ACK, // 回应
    REPL_STATE_NULL // 无效状态
};

// AOF 文件名相关定义
const std::string BASE_AOF_FILE_NAME = "base_aof.txt";
const std::string INCR_AOF_FILE_NAME = "incr_aof.txt";
const std::string TEMP_BASE_AOF_FILE_NAME = "temp_base_aof.txt";
const std::string TEMP_INCR_AOF_FILE_NAME = "temp_incr_aof.txt";
struct ConnectionPack
{
    size_t offset;
    ReplStatus status;
};

// 服务器 配置信息
struct ServerConfig
{
    std::string serverID;
    bool isSlave;  // 自己是不是从机
    std::string mastr_IP, slave_IP; // 主机/从机的IP
    uint64_t master_port, slave_port; // 主机/从机的端口号
    int master_socket_fd,slave_socket_fd; // 主机从机套接字文件描述符
    ConnectionPack conn; // 握手发送的包

    std::string dumpDir; // 持久化文件的路径

};


// 命令编号
enum CMD_FLAG {
    CMD_SET = 0,
    CMD_GET,
    CMD_BGSAVE,
    CMD_SYNC,
    CMD_AOF_REWRIIE,
    CMD_BGSAVE
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
    int clear();
    Command& at(size_t index);
    Command& front();

private:
    std::vector<Command> v;
    int _start;
    int _end;
    int _size;
    int _capacity;
};

class DataBase
{
public:
    DB db;
    ServerConfig config;

    // 主从复制命令缓存
    CmdBuff cmdbuff;
    // incr_aof.txt 文件写入流
    std::ofstream incrAofStream;
    // aof_buff
    CmdBuff aof_buff;
public:
    // 构造函数
    DataBase():db(6),cmdbuff(REPL_BUFF_LEN),incrAofStream(),aof_buff(AOF_BUFF_LEN) {}

    // 初始化数据库服务器
    void dataBaseInit();
};



// 执行相关命令
void execCommand(DataBase &db, Command cmd);


#endif // REDIS_LEARN_COMMON