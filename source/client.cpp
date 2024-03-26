#include <iostream>
#include <queue>
#include <string.h>
#include <fstream>
#include <thread>
#include <fstream>
#include <chrono>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <chrono>
using namespace std;




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

size_t getBinaryCmd(char *buff, CMD_FLAG cmd_flag, std::string key, std::string value = std::string())
{
    char *p = buff;

    *(CMD_FLAG*)p =  cmd_flag;
    p = p + sizeof(CMD_FLAG);

    int keyLen = key.length();
    *(size_t*)p = keyLen+1;
    p = p + sizeof(size_t);

    memcpy(p, key.c_str(), keyLen);
    p = p + keyLen;
    *p = '\0';
    ++p;

    int valLen = value.length();
    *(size_t*)(p) = valLen+1;
    p = p + sizeof(size_t);

    if(valLen != 0) memcpy(p,value.c_str(),valLen);
    p += valLen;
    *p = '\0';

    return sizeof(CMD_FLAG) + 2 * sizeof(size_t) + keyLen + valLen + 2;
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

std::string sendCmd(int sock, CMD_FLAG flag, std::string key = std::string(), std::string value = std::string())
{
    char buff[1024];
    size_t len = getBinaryCmd(buff, flag, key, value);
    write(sock,&len, sizeof(size_t));
    //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write(sock, buff, len);
    //for(;;);
    read(sock,&len, sizeof(size_t));
    read(sock,buff,len);
    buff[len]='\0';
    cout<<buff<<endl;
    return string(buff);
    //this_thread::sleep_for(chrono::milliseconds(1000));
}

void connectToMaster()
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) errorHandling("socket get error!");
    sockaddr_in master_adr;
    memset(&master_adr, 0, sizeof(master_adr));
    master_adr.sin_family = AF_INET;
    master_adr.sin_addr.s_addr = inet_addr("127.0.0.1");
    master_adr.sin_port = htons(9000);

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    int serverFd;
    if(connect(sock, reinterpret_cast<sockaddr *>(&master_adr), sizeof(master_adr)))
    {
        errorHandling("connect error!");
    }
    //std::cout<<"connect success!"<<endl;

    sendCmd(sock, CMD_SET, "hello","world");
    sendCmd(sock, CMD_SET, "myc","66666");
    sendCmd(sock, CMD_SET, "zsx","world");
    sendCmd(sock, CMD_SET, "mkx","world");
    sendCmd(sock, CMD_SET, "whb","world");
    sendCmd(sock, CMD_SET, "abc","world");
    sendCmd(sock, CMD_SET, "ddd","aaa");
    sendCmd(sock,CMD_GET, "ddd");
    sendCmd(sock,CMD_GET, "myc");
    // sendCmd(sock,CMD_SHUTDOWN);

    return;
}

int main()
{
    std::chrono::milliseconds start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    int n = 30;
    vector<thread> tv(0);
    for(int i=0;i<n;++i)
    {
        tv.emplace_back(thread(connectToMaster));
    }
    for(int i=0;i<n;++i) tv[i].join();
    std::chrono::milliseconds end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    //std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout<<(end-start).count()<<endl;
    return 0;
}
