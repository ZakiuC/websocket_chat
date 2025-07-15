#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <set>
#include <thread>
#include <mutex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::set<std::shared_ptr<Session>>& sessions_;
    std::mutex& sessions_mutex_;

public:
    explicit Session(tcp::socket&& socket, std::set<std::shared_ptr<Session>>& sessions, std::mutex& mutex)
        : ws_(std::move(socket)), sessions_(sessions), sessions_mutex_(mutex) {}

    void run() {
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));
        
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server,
                    "WebSocket-Server");
            }));
        
        ws_.async_accept(
            beast::bind_front_handler(
                &Session::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if(ec) {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_.insert(shared_from_this());
            std::cout << "New client connected. Total clients: " << sessions_.size() << std::endl;
        }
        
        read_message();
    }

    void read_message() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &Session::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t) {
        if(ec == websocket::error::closed) {
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.erase(shared_from_this());
                std::cout << "Client disconnected. Total clients: " << sessions_.size() << std::endl;
            }
            return;
        }
        
        if(ec) {
            std::cerr << "Read error: " << ec.message() << std::endl;
            return;
        }
        
        // 获取消息内容
        auto out = beast::buffers_to_string(buffer_.data());
        std::cout << "Received message: " << out << std::endl;
        
        // 广播消息给所有客户端
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
        
        buffer_.consume(buffer_.size());
        read_message();
    }
};

class Server {
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::set<std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;

public:
    Server(unsigned short port)
        : acceptor_(ioc_, {tcp::v4(), port}) {
        accept_connection();
    }

    void run() {
        std::cout << "WebSocket server listening on port 8080\n";
        ioc_.run();
    }

private:
    void accept_connection() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if(!ec) {
                    std::make_shared<Session>(std::move(socket), sessions_, sessions_mutex_)->run();
                } else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }
                accept_connection();
            });
    }
};

int main() {
    try {
        Server server(8080);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}