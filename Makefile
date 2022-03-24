CXX = g++

CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server

# OBJS += log/*.cpp    
# OBJS += config/*.cpp    
# OBJS += pool/*.cpp   
# OBJS += timer/*.cpp  
# OBJS += http/*.cpp   
# OBJS += server/*.cpp 
# OBJS += buffer/*.cpp 
OBJS += main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ./bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ./bin/*




