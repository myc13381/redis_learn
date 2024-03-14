#ifndef REDIS_LEARN_AOF
#define REDIS_LEARN_AOF

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

#include "common.h"



// 将 aof_buff 中de命令写入到 ofs 文件中，一般是 INCR_AOF
void writeInrcAofFile(CmdBuff &aof_buff, std::ofstream &ofs);

// 重写 BASE_AOF 文件 重写到 targetFile 文件中，做法是遍历数据库，然后依次写入文件
void reWriteBaseAofFile(DataBase &db, std::string targetFile);

// 重写 AOF 的整个流程
void AOFRW(DataBase &db);

#endif // REDIS_LEARN_AOF