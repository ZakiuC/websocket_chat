### websocket


#### 安装依赖

#### Boost

```bash
sudo apt install aptitude   # 安装aptitude自动处理依赖冲突

sudo aptitude libboost-all-dev

sudo apt install libboost-dev libboost-system-dev   # 没装完整可以手动再装一遍
```

#### 结构

websocket_chat/  
├── server/  
│   ├── CMakeLists.txt  
│   └── websocket_server.cpp  
├── client/  
│   ├── CMakeLists.txt  
│   └── websocket_client.cpp  
└── build/  
    ├── server  
    └── client  

#### 使用

##### 编译

###### 创建build目录

```bash
mkdir -p build/{server,client}
```

###### 服务端

```bash
cd build/server

cmake ../../server

make -j4
```

###### 客户端

```bash
cd ../client

cmake ../../client

make -j4
```

##### 使用

```bash
# 在第一个终端运行服务器
cd build/server
./websocket_server

# 在第二个终端运行第一个客户端
cd ../client
./websocket_client 127.0.0.1 8080

# 在第三个终端运行第二个客户端
cd ../client
./websocket_client 127.0.0.1 8080
```

###### 说明

1. **服务器**：
   - 监听本地8080端口
   - 显示连接/断开客户端的日志
   - 广播所有消息到其他客户端

2. **客户端**：
   - 连接格式：`./websocket_client <host> <port>`
   - 输入消息后按回车发送
   - 输入 "exit" 退出程序
   - 接收消息时会显示在单独行中
