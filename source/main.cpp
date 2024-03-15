//所有函数的解释与用法都在skiplist.h中，main主函数主要用于测试各种函数是否可行

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include "skiplist.h"
#include "hyperLogLog.h"
#include "common.h"
#include "replication.h"
#include "aof.h"

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
    ServerConfig sc;
    sc.isSlave = false;
    sc.slave_IP = sc.mastr_IP = "127.0.0.1";
    sc.slave_port = 9000;
    sc.master_port = 8001;
    sc.conn.offset=7777;
    connectToSlave(sc);
    shakeHandWithSlave(sc);
    //sendFile(sc,"/home/myc/Desktop/master.txt");
    disconnectToSlave(sc);

}

void slaveTest()
{
    ServerConfig sc;
    sc.isSlave = false;
    sc.slave_IP = sc.mastr_IP = "127.0.0.1";
    sc.slave_port = 9000;
    sc.master_port = 8001;
    sc.conn.offset=6666;
    connectToMaster(sc);
    shakeHandWithMaster(sc);
    //recvFile(sc, "/home/myc/Desktop/slave.txt");
    disconnectoMaster(sc);

}

int main()
{
    // __pid_t pid = fork();
    // if(pid == 0)
    // {
    //     // child
    //     masterTest();
    // }
    // else 
    // {
    //     // parent;

    //     slaveTest();
    //     int status;
    //     wait(&status);
        
    // }
    // return 0;
    DataBase db;
    AOFRW(db);
    for(int i=0;i<10000;++i);
}