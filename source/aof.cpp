#include "aof.h"

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
void reWriteBaseAofFile(DataBase db, std::string targetFile)
{
    std::ofstream ofs;
    ofs.open(targetFile,std::ios::trunc);
    if(ofs.is_open())
    {
        // for(int i=0;i<100;++i) ofs<<i<<' ';
        // ofs<<'\n';

        DBNode *node = const_cast<DBNode*>(db.db.getHeader())->forward[0];
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
void AOFRW(DataBase &db)
{
    // 关闭之前写入 incr_aof_file 的ofstream，将原来的文件重命名，然后产生新的 incr_aof_file 文件
    if(db.incrAofStream.is_open()) db.incrAofStream.close();
    std::string oldIncrName = "../"+INCR_AOF_FILE_NAME;
    std::string tempIncrName = "../"+TEMP_INCR_AOF_FILE_NAME;
    // 文件重命名
    rename(oldIncrName.c_str(),tempIncrName.c_str());
    

    // 将之前的 base_aof_file 重命名，然后开辟子线程用来重写base_aof_file
    std::string oldBaseName = "../"+BASE_AOF_FILE_NAME;
    std::string tempBaseName = "../"+TEMP_BASE_AOF_FILE_NAME;
    rename(oldBaseName.c_str(),tempBaseName.c_str());
    std::thread childThread(reWriteBaseAofFile, db, oldBaseName);
    childThread.detach();
    // 然后产生新的 incr_aof_file 文件, ofstream 指向新的文件，父进程继续添加AOF文件
    // 删除以前的文件
    remove(tempBaseName.c_str()); 
    remove(tempIncrName.c_str());
    db.incrAofStream.open(oldIncrName,std::ios::trunc);
    
}