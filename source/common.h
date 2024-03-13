#ifndef REDIS_LEARN_COMMON
#define REDIS_LEARN_COMMON
#include <vector>
#include <string>
#include "skiplist.h"

typedef SkipList<std::string,std::string> DB; // 数据库类型

constexpr size_t INCR_LEN = 128;

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


struct ConnectionPack
{
    size_t offset;
    ReplStatus status;
};

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

class DataBase
{
public:
    DB db;
    ServerConfig config;

    DataBase():db(6){}
};


#endif // REDIS_LEARN_COMMON