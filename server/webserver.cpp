#include "webserver.h"

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, 
                    int sqlPort, const char* sqlUser, const  char* sqlPwd, 
                    const char* dbName, int connPoolNum, int threadNum,
                    bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    
    //1. 初始化数据库、连接池相关
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    printf("SqlConnPool::Instance()->Init,isClose_ = %d \n", isClose_);
    //2. 设置事件触发模式
    InitEventMode_(trigMode);
    
    //3. 初始化监听套接字、epoll
    if(!InitSocket_()) 
        isClose_ = true;
    printf("InitSocket_,isClose_ = %d \n", isClose_);
    //4. 初始化日志系统
    if(openLog){
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_){
            LOG_ERROR("========== Server init error!==========");
        }
        else{
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::Start(){
    int timeMS = -1;
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        if(timeoutMS_ > 0){
            timeMS = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS);
        
        for(int i = 0; i < eventCnt; ++i){
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_){
                // 处理监听事件
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 错误，关闭连接
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN){
                // 处理读事件
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT){
                // 处理写事件
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else{
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

bool WebServer::InitSocket_(){
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, '\0', sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0){
        perror("socket() error!");
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    // 设置非阻塞
    SetFdNonblock(listenFd_);

    // 设置优雅的关闭套接字
    struct linger optLinger = {0};
    if(openLinger_){
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    int ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(struct linger));
    if(ret < 0){
        close(listenFd_);
        perror("setsockopt() error!");
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    // 设置端口复用
    int reUse = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reUse, sizeof(int));
    if(ret == -1) {
        perror("setsockopt() error!");
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
    if(ret < 0) {
        perror("bind() error!");
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 32);
    if(ret < 0){
        perror("listen() error!");
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    if(!epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN)){
        LOG_ERROR("Add listenFd_ error!");
        perror("Add listenFd_ error!");
        close(listenFd_);
        return false;
    }

    printf("Server port:%d", port_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::SetFdNonblock(int fd){
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::AddClient_(int fd, sockaddr_in addr){
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0){
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(sockaddr_in);
    do{
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &addrLen);
        if(fd <= 0) return;
        else if(HttpConn::userCount >= MAX_FD){
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    }while(listenEvent_ & EPOLLET);  
}

void WebServer::CloseConn_(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::DealRead_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::SendError_(int fd, const char*info){
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::ExtentTime_(HttpConn* client){
    assert(client);
    if(timeoutMS_ > 0){
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

void WebServer::OnRead_(HttpConn* client){
    assert(client);
    int ret = -1;
    int readErrno = 0;

    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN){
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnWrite_(HttpConn* client){
    assert(client);
    int ret = -1;
    int writeErrno = 0;

    ret = client->write(&writeErrno);
    LOG_DEBUG("WRITE bytes %d\n", ret);
    if(client->ToWriteBytes() == 0){ // 传输完成
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0){
        if(writeErrno == EAGAIN) {  // 继续传输
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnProcess(HttpConn* client){
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

