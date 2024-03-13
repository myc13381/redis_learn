// 主从复制
// 第一步配置数据，数据库启动后获得主/从 的IP，port
// 如果是主机，第一次主动连接从机，并将本地文件发送给从机
// 如果是从机，则不断监听主机发送的请求

#ifndef REDIS_LEARN_REPLICATION
#define REDIS_LEARN_REPLICATION

#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "skiplist.h"

const std::string PING = "PING";
const std::string PONG = "PONG";

// =====================master=============================
bool connectToSlave(ServerConfig &config); // 主机连接从机

void disconnectToSlave(ServerConfig &config); // 主机断开从机

bool slaveIsConnected(ServerConfig &config); // 判断从机是否正常连接，发送心跳包

ConnectionPack shakeHandWithSlave(ServerConfig &config); // 和从机握手，用于检测连接是否正常


void sendToSlave(ServerConfig &config); // 向 slave 发送包

void syncWithSlave(DataBase &db); // 和从机同步

void sendFile(ServerConfig &config, std::string fileName); // 发送文件


//======================slave=============================
bool connectToMaster(ServerConfig &config); // 从机等待主机连接

void disconnectoMaster(ServerConfig &config); // 从机断开连接

ConnectionPack shakeHandWithMaster(ServerConfig &config); // 和主机握手

void sendToMaster(ServerConfig &config); // 向 master 发送包

void syncWithMaster(DataBase &db); // 和主机同步

void recvFile(ServerConfig &config, std::string fileName); // 接收文件

#endif // REDIS_LEARN_REPLICATION
