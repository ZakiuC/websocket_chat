#define BOOST_BEAST_USE_STD_STRING_VIEW  // 使用标准库的 std::string_view，方便 Beast 库使用

#include <boost/beast/core.hpp>            // 引入 Boost.Beast 核心库
#include <boost/beast/websocket.hpp>       // 引入 WebSocket 支持
#include <boost/asio/connect.hpp>         // 引入 Boost.Asio 连接功能
#include <boost/asio/ip/tcp.hpp>          // 引入 Boost.Asio TCP 支持
#include <cstdlib>                         // 引入 EXIT_SUCCESS、EXIT_FAILURE 等
#include <iostream>                        // 标准输入输出流
#include <string>                          // std::string 类型
#include <thread>                          // 多线程支持
#include <atomic>                          // 原子操作支持
#include <mutex>                           // 互斥量支持

// 为不同模块定义简短别名，减少代码冗余
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// WebSocket 客户端类，负责连接、发送、接收和断开等操作
class WebSocketClient {
    net::io_context ioc_;                        // I/O 上下文，用于管理异步操作
    websocket::stream<tcp::socket> ws_;          // WebSocket 流，用于读写消息
    std::string host_;                           // 服务器主机地址
    std::string port_;                           // 服务器端口
    std::thread receive_thread_;                 // 接收消息的后台线程
    std::atomic<bool> running_{true};            // 运行状态标志，用于控制循环
    std::mutex cout_mutex_;                      // 保护 std::cout 的互斥量，避免多线程交叉输出

public:
    // 构造函数：初始化 WebSocket 流和服务器地址端口
    WebSocketClient(const std::string& host, const std::string& port)
        : ws_(ioc_), host_(host), port_(port) {}
    
    // 析构函数：确保断开连接并清理后台线程
    ~WebSocketClient() {
        disconnect();
    }
    
    // 建立连接并完成 WebSocket 握手
    void connect() {
        // 解析主机名和端口
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host_, port_);
        
        // 连接到第一个可用的 endpoint
        net::connect(ws_.next_layer(), results.begin(), results.end());
        
        // 完成 WebSocket 握手，路径为 '/'
        ws_.handshake(host_, "/");
        
        // 输出连接成功信息（加锁以避免并发输出混乱）
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << "Connected to server at " << host_ << ":" << port_ << std::endl;
        }
    }
    
    // 断开连接并停止接收线程
    void disconnect() {
        if (running_) {
            running_ = false;  // 停止接收循环
            
            // 优雅关闭 WebSocket 连接
            beast::error_code ec;
            ws_.close(websocket::close_code::normal, ec);
            
            // 如果关闭过程中有错误，输出提示
            if (ec) {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cerr << "Close error: " << ec.message() << std::endl;
            }
            
            // 等待接收线程结束
            if (receive_thread_.joinable()) {
                receive_thread_.join();
            }
        }
    }
    
    // 发送字符串消息到服务器
    void send(const std::string& message) {
        ws_.write(net::buffer(message));
    }
    
    // 接收服务器消息的循环函数，将运行在单独线程
    void receive() {
        beast::flat_buffer buffer;
        while (running_) {
            beast::error_code ec;
            ws_.read(buffer, ec);  // 阻塞读取
            
            // 如果服务器关闭连接
            if (ec == websocket::error::closed) {
                if (running_) {
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cout << "\nConnection closed by server" << std::endl;
                }
                break;
            } else if (ec) {
                // 其他读取错误
                if (running_) {
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cerr << "\nRead error: " << ec.message() << std::endl;
                }
                break;
            }
            
            // 将缓冲区数据转换为字符串并输出
            auto msg = beast::buffers_to_string(buffer.data());
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << "\nReceived: " << msg << std::endl;
                std::cout << "Enter message: " << std::flush;
            }
            buffer.consume(buffer.size());  // 清空缓冲区
        }
    }
    
    // 主运行逻辑，启动接收线程并处理用户输入
    void run() {
        // 启动后台线程执行 receive()
        receive_thread_ = std::thread([this]() { receive(); });
        
        std::string input;
        while (running_) {
            // 提示输入
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << "Enter message: ";
            }
            
            // 读取用户输入
            if (!std::getline(std::cin, input)) {
                // 输入结束（如 Ctrl+D），退出循环
                break;
            }
            
            // 输入 exit 时退出
            if (input == "exit") {
                break;
            }
            
            // 发送消息
            send(input);
        }
        
        // 退出时断开连接
        disconnect();
    }
};

// 程序入口：接收命令行参数并启动客户端
int main(int argc, char** argv) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 8080\n";
        return EXIT_FAILURE;
    }
    
    try {
        WebSocketClient client(argv[1], argv[2]);  // 创建客户端实例
        client.connect();                          // 建立连接并握手
        client.run();                              // 运行发送/接收循环
    } catch (const std::exception& e) {
        // 捕获并输出异常
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
