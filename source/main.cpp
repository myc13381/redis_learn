//所有函数的解释与用法都在skiplist.h中，main主函数主要用于测试各种函数是否可行

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include "skiplist.h"
#include "hyperLogLog.h"
#include "server.h"
#include "dict.h"
#include "ae.h"

#ifdef _WIN32
    #define STORE_FILE "../dumpfile"
#else 
    #define STORE_FILE "/home/myc/Desktop/project/Redis-SkipList-main/dumpfile"
#endif

void SkipTest()
{
    SkipList<std::string ,std::string>skipList(6);
    skipList.display_list();
    skipList.insert_element("1","学习");
    skipList.insert_element("3","跳表");
    skipList.insert_element("7","去找");
    skipList.insert_element("8","GitHub:");
    skipList.insert_element("9","myc13381");
    skipList.insert_element("20","赶紧给个");
    skipList.insert_element("20","star!");
    std::cout<<"skipList.size = "<<skipList.size()<<std::endl;
    skipList.dump_file(STORE_FILE);
    skipList.search_element("8");
    skipList.search_element("9");
    skipList.display_list();
    skipList.delete_element("3");
    skipList.load_file(STORE_FILE);
    std::cout<<"skipList.size = "<<skipList.size()<<std::endl;
    skipList.display_list();
    skipList.clear();
    skipList.insert_element("1","学习");
    skipList.insert_element("3","跳表");
    skipList.insert_element("7","去找");
    skipList.display_list();
}


void HyperLogLogTest()
{
    HyperLogLog hll;
    //hll.hllSparseToDense();
    std::string str("hello");
    hll.hllAdd(str);
    str="hll";
    hll.hllAdd(str);
    for(int i=0;i<26;++i)
    {
        str="myc"+char(i+'a');
        hll.hllAdd(str);
    }
    int invalid;
    uint64_t ret = hll.hllCount(invalid);
    std::cout<<ret<<'\n';
    std::vector<uint8_t> reg(HLL_REGISTERS,0);
    hll.hllMerge(reg);
}

void masterTest()
{
    Server server;
    server.config.isSlave = false;
    server.config.slave_IP = server.config.master_IP = "127.0.0.1";
    server.config.slave_port = 9000;
    server.config.master_port = 8001;
    server.config.conn.offset=7777;
    connectToSlave(server.config);
    //shakeHandWithSlave(server.config);
    syncWithSlave(server);


}

void slaveTest()
{
    Server server;
    ServerConfig sc;
    sc.isSlave = false;
    sc.slave_IP = sc.master_IP = "127.0.0.1";
    sc.slave_port = 9000;
    sc.master_port = 8001;
    sc.conn.offset=6666;
    server.config = sc;
    connectToMaster(sc);
    //shakeHandWithMaster(sc);
    syncWithMaster(server);
    disconnectoMaster(sc);

}

void dictTest()
{
    auto show = [](HashNode *node)
    {
        if(node==nullptr) std::cout<<"nullptr\n";
        else std::cout<<node->getKey()<<" "<<node->getValue()<<'\n';
    };
    // test hashtable
    std::cout<<"===========Hashtable==============\n";
    Hashtable t1;
    HashNode *node = nullptr;
    node = t1.find("hello");
    show(node);
    t1.insert("hello","myc");
    node = t1.find("hello");
    show(node);
    t1.erase("hello");
    t1.erase("hello");
    t1.erase("hello");
    t1.insert("world","zzzz");
    t1.insert("world","hello");
    show(t1.find("world"));

    // test dict
    std::cout<<"============Dict=================\n";
    Dict dict;
    dict.insert("hello","world");
    show(dict.find("myc"));
    show(dict.find("hello"));
    show(dict.erase("hello"));
    show(dict.erase("zyg"));
    return;
    
}

void serverTest()
{
    aeEventLoop loop;
    Server server;
    // 初始化epoll
    aeApiCreate(loop);
    // 初始化服务器
    server.ServerInit();
    //aeCreateFileEvent(server.config.master_socket_fd,loop,aeServerConnectToClient,AE_READABLE,nullptr);
    aeApiAddEvent(loop, server.config.master_socket_fd, AE_READABLE); // 将服务器监听套接字加入epoll

    aeMain(server,loop);

}

int main()
{

    serverTest();
    return 0;
}