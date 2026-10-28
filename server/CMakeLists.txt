cmake_minimum_required(VERSION 3.18)
project(chat_server LANGUAGES CXX)

# ---------- 全局编译选项 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认 Debug (-g, 与原来一致); 发布时用 -DCMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 默认安装到构建目录, make install 后程序在 build/bin 下 (与原来一致)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR} CACHE PATH "安装路径" FORCE)
endif()

option(CHAT_BUILD_TESTS "编译各服务的测试客户端" ON)

set(CHAT_SERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/chat.cmake)

# ---------- 公共代码 (只编译一次) ----------
add_subdirectory(proto)
add_subdirectory(odb)

# ---------- 各子服务 ----------
foreach(service user friend message transmite channel gateway file speech)
    add_subdirectory(${service})
endforeach()
