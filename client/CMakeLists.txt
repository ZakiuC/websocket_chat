cmake_minimum_required(VERSION 3.10)
project(websocket_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# 手动设置 Boost 路径
set(BOOST_INCLUDEDIR "/usr/include")
include_directories(${BOOST_INCLUDEDIR})

add_executable(websocket_client websocket_client.cpp)

# 链接库
target_link_libraries(websocket_client PRIVATE 
    boost_system
    pthread
)