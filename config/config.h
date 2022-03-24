#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

class Config{
public:
    Config(){}
    
    ~Config(){}

    int ParseCmd(int argc, char *argv[]);

    int SetDaemon();

};

#endif 

