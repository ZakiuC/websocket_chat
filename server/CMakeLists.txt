# 指定使用的 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 定义工程名称
project(websocket_server)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)

# 为编译器添加 -pthread 选项，以支持多线程
# 注意：如果使用的是 MSVC 编译器，则不需要 pthread
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# -------------------------------------------------------------------
# 如果你的 Boost 安装在非标准位置，可以手动指定 Boost 头文件目录
# 这里假设 Boost 安装在 /usr/include，通常系统默认就能找到
# 如果你的路径不同，请修改为对应路径
set(BOOST_INCLUDEDIR "/usr/include")
# 将 BOOST_INCLUDEDIR 添加到头文件搜索路径中
include_directories(${BOOST_INCLUDEDIR})

# -------------------------------------------------------------------
# 添加可执行文件，指定源文件
add_executable(websocket_client websocket_client.cpp)

# -------------------------------------------------------------------
# 链接所需的系统库
#  - boost_system: Boost.Asio 和 Boost.Beast 需要它来处理底层系统调用
#  - pthread: POSIX 线程库（只有在 Linux/macOS 上需要）
target_link_libraries(websocket_client PRIVATE 
    boost_system
    pthread
)