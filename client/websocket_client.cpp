#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketClient {
    net::io_context ioc_;
    websocket::stream<tcp::socket> ws_;
    std::string host_;
    std::string port_;
    std::thread receive_thread_;
    std::atomic<bool> running_{true};
    std::mutex cout_mutex_;
    
public:
    WebSocketClient(const std::string& host, const std::string& port)
        : ws_(ioc_), host_(host), port_(port) {}
    
    ~WebSocketClient() {
        disconnect();
    }
    
    void connect() {
        tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host_, port_);
        
        net::connect(ws_.next_layer(), results.begin(), results.end());
        
        ws_.handshake(host_, "/");
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cout << "Connected to server at " << host_ << ":" << port_ << std::endl;
        }
    }
    
    void disconnect() {
        if (running_) {
            running_ = false;
            
            // 关闭 WebSocket 连接
            beast::error_code ec;
            ws_.close(websocket::close_code::normal, ec);
            
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
    
    void send(const std::string& message) {
        ws_.write(net::buffer(message));
    }
    
    void receive() {
        beast::flat_buffer buffer;
        while (running_) {
            beast::error_code ec;
            ws_.read(buffer, ec);
            
            if (ec == websocket::error::closed) {
                if (running_) {
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cout << "\nConnection closed by server" << std::endl;
                }
                break;
            } else if (ec) {
                if (running_) {
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cerr << "\nRead error: " << ec.message() << std::endl;
                }
                break;
            }
            
            auto msg = beast::buffers_to_string(buffer.data());
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << "\nReceived: " << msg << std::endl;
                std::cout << "Enter message: " << std::flush;
            }
            buffer.consume(buffer.size());
        }
    }
    
    void run() {
        receive_thread_ = std::thread([this]() { receive(); });
        
        std::string input;
        while (running_) {
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cout << "Enter message: ";
            }
            
            if (!std::getline(std::cin, input)) {
                // 处理输入结束（如 Ctrl+D）
                break;
            }
            
            if (input == "exit") {
                break;
            }
            
            send(input);
        }
        
        disconnect();
    }
};

int main(int argc, char** argv) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 8080\n";
        return EXIT_FAILURE;
    }
    
    try {
        WebSocketClient client(argv[1], argv[2]);
        client.connect();
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}