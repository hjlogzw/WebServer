#include <stdio.h>
#include <sys/wait.h>
#include "./config/config.h"
#include "./server/webserver.h"

int main(int argc, char *argv[]){
    Config config;
    int ret = config.ParseCmd(argc, argv);
    if(ret != 0){
        return -1;
    }
    
    WebServer server(
        1317, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "713999", "webserver", /* Mysql配置 */
        12, 6, true, 0, 1024);             /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    server.Start();

    return 0;
}