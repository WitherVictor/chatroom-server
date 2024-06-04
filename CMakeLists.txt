cmake_minimum_required(VERSION 3.0.0)
project(chatroom-server VERSION 0.1.0 LANGUAGES C CXX)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_compile_options(-std=c++23)
add_executable(chatroom-server main.cpp)

# 引入 Boost
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(chatroom-server ${Boost_LIBRARIES})

# 引入 fmt, 作为 spdlog 的依赖库
target_link_libraries(chatroom-server PRIVATE fmt::fmt-header-only)

# 引入 spdlog, 作为日志输出库
target_link_libraries(chatroom-server PRIVATE spdlog::spdlog_header_only)

# 引入 Boost.asio 依赖库
target_link_libraries(chatroom-server PRIVATE ws2_32)