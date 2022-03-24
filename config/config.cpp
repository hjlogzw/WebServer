#include "config.h"
#include "../log/log.h"
#include <stdio.h>

int Config::ParseCmd(int argc, char *argv[]){
    int ret;

    while((ret = getopt(argc, argv, "d:")) != -1){
        switch(ret){
            case 'd':
                if(atoi(optarg) == 1){
                    return SetDaemon();
                }
                break;
            default:
                break;
        }
    }
    return -1;
}

// 1：父进程返回   0：子进程返回   -1：发生错误  
int Config::SetDaemon(){

    switch(fork()){
        case -1:
            printf("SetDaemon() failed! fork() failed!");
            return -1;
        case 0:
            break;
        default:
            return 1;
    }

    // 设置会话组，脱离终端
    if(setsid() == -1){
        printf("SetDaemon() failed! setsid() failed!");
        return -1;
    }

    // 重设文件权限掩码
    umask(0);

    int fd = open("/dev/null", O_RDWR);
    if(fd == -1){
        printf("SetDaemon() failed! open() failed!");
        return -1;
    }

    // 关闭文件描述符
    if(dup2(fd, STDIN_FILENO) == -1) // 关闭并重定向标准输入为fd
    {
        printf("SetDaemon()中dup2(STDIN)失败!");        
        return -1;
    }
    if(dup2(fd, STDOUT_FILENO) == -1)
    {
        printf("SetDaemon()中dup2(STDOUT)失败!");
        return -1;
    }
    if(fd > STDERR_FILENO)
    {
        if(close(fd) == -1)
        {
            printf("SetDaemon()中close(fd)失败!");
            return -1;
        }
    }

    return 0;
}
