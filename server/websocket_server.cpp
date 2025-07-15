#define BOOST_BEAST_USE_STD_STRING_VIEW  // 使用标准库的 std::string_view，以便 Beast 使用

#include <boost/beast/core.hpp>          // 引入 Boost.Beast 核心功能
#include <boost/beast/websocket.hpp>     // 引入 WebSocket 支持
#include <boost/asio/ip/tcp.hpp>         // 引入 Boost.Asio TCP 支持
#include <cstdlib>                       // 引入通用工具（如 EXIT_SUCCESS）
#include <functional>                    // 引入 std::function 等
#include <iostream>                      // 标准输入输出流
#include <memory>                        // 智能指针支持
#include <string>                        // std::string 支持
#include <set>                           // std::set 容器
#include <thread>                        // 多线程支持
#include <mutex>                         // 互斥量支持

// 为不同模块定义别名，简化后续代码书写
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// 会话类，表示与单个客户端的 WebSocket 连接
class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;          // WebSocket 流，用于读写消息
    beast::flat_buffer buffer_;                        // 缓冲区，用于存储接收的数据
    std::set<std::shared_ptr<Session>>& sessions_;     // 全部会话集合引用，用于广播
    std::mutex& sessions_mutex_;                       // 保护 sessions_ 的互斥量引用

public:
    // 构造函数：接收一个已连接的 socket、会话集合和互斥量引用
    explicit Session(tcp::socket&& socket, std::set<std::shared_ptr<Session>>& sessions, std::mutex& mutex)
        : ws_(std::move(socket)), sessions_(sessions), sessions_mutex_(mutex) {}

    // 启动会话，设置选项并接受 WebSocket 握手
    void run() {
        // 设置超时选项，使用推荐的服务器角色超时配置
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));
        
        // 设置握手响应头装饰器，添加 Server 字段
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server,
                    "WebSocket-Server");
            }));
        
        // 异步接受握手，完成后调用 on_accept
        ws_.async_accept(
            beast::bind_front_handler(
                &Session::on_accept,
                shared_from_this()));
    }

    // 握手完成后的回调
    void on_accept(beast::error_code ec) {
        if(ec) {
            // 握手错误时输出并返回
            std::cerr << "Accept error: " << ec.message() << std::endl;
            return;
        }
        
        // 将当前会话加入到全局集合，保护操作加锁
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_.insert(shared_from_this());
            std::cout << "New client connected. Total clients: " << sessions_.size() << std::endl;
        }
        
        // 开始读取消息
        read_message();
    }

    // 异步读取消息
    void read_message() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &Session::on_read,
                shared_from_this()));
    }

    // 读取完成后的回调
    void on_read(beast::error_code ec, std::size_t) {
        if(ec == websocket::error::closed) {
            // 如果客户端关闭连接，从集合中移除并返回
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.erase(shared_from_this());
                std::cout << "Client disconnected. Total clients: " << sessions_.size() << std::endl;
            }
            return;
        }
        
        if(ec) {
            // 其他读取错误时输出并返回
            std::cerr << "Read error: " << ec.message() << std::endl;
            return;
        }
        
        // 将缓冲区中的数据转换为字符串
        auto out = beast::buffers_to_string(buffer_.data());
        std::cout << "Received message: " << out << std::endl;
        
        // 广播消息给所有其他客户端
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for(auto& session : sessions_) {
                if(session.get() != this) {
                    session->ws_.async_write(
                        net::buffer(out),
                        [](beast::error_code ec, std::size_t bytes_transferred) {
                            if(ec) {
                                std::cerr << "Write error: " << ec.message() << std::endl;
                            }
                        });
                }
            }
        }
        
        // 清空缓冲区并继续读取下一条消息
        buffer_.consume(buffer_.size());
        read_message();
    }
};

// 服务器类，负责监听端口并接受连接
class Server {
    net::io_context ioc_;                             // I/O 上下文，用于管理异步操作
    tcp::acceptor acceptor_;                         // TCP 接受器，用于监听新连接
    std::set<std::shared_ptr<Session>> sessions_;     // 存储所有会话
    std::mutex sessions_mutex_;                      // 保护 sessions_ 的互斥量

public:
    // 构造函数：在指定端口创建接受器并启动接受连接流程
    Server(unsigned short port)
        : acceptor_(ioc_, {tcp::v4(), port}) {
        accept_connection();
    }

    // 运行服务器事件循环
    void run() {
        std::cout << "WebSocket server listening on port 8080\n";
        ioc_.run();
    }

private:
    // 异步接受连接，并为每个新连接创建一个 Session
    void accept_connection() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if(!ec) {
                    std::make_shared<Session>(std::move(socket), sessions_, sessions_mutex_)->run();
                } else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }
                // 继续接受下一次连接
                accept_connection();
            });
    }
};

// 程序入口点
int main() {
    try {
        Server server(8080);  // 在 8080 端口启动服务器
        server.run();         // 运行 I/O 循环
    } catch (const std::exception& e) {
        // 捕获并输出任何异常
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
