# chat_server 公共构建配置: 第三方依赖分组 + 服务/测试目标封装

# ---------- 代码生成工具 ----------
find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(ODB_EXECUTABLE odb REQUIRED)
find_package(Threads REQUIRED)

# ---------- 第三方库 ----------
# 仍按库名链接 (等价于原来的 -lxxx, 实际链接的库文件不变),
# 但 configure 阶段先检查一遍, 缺库立即报错, 而不是等到链接阶段报一堆 undefined reference
function(chat_check_libs)
    foreach(lib ${ARGN})
        string(MAKE_C_IDENTIFIER "CHAT_LIB_${lib}" var)
        find_library(${var} NAMES ${lib})
        if(NOT ${var})
            message(FATAL_ERROR "找不到依赖库 lib${lib}, 请先安装")
        endif()
    endforeach()
endfunction()

# 顺序与原来的 -l 顺序保持一致 (brpc 是静态库, 其依赖必须排在它后面)
set(CHAT_BASE_LIBS  gflags spdlog fmt brpc ssl crypto protobuf leveldb
                    etcd-cpp-api cpprest curl jsoncpp)
set(CHAT_ODB_LIBS   odb-mysql odb odb-boost mysqlclient)
set(CHAT_ES_LIBS    cpr elasticlient)
set(CHAT_REDIS_LIBS hiredis redis++)
set(CHAT_MQ_LIBS    amqpcpp ev)

chat_check_libs(${CHAT_BASE_LIBS} ${CHAT_ODB_LIBS} ${CHAT_ES_LIBS}
                ${CHAT_REDIS_LIBS} ${CHAT_MQ_LIBS} boost_system)
if(CHAT_BUILD_TESTS)
    chat_check_libs(gtest)
endif()

# chat::base  所有服务共用: rpc / 服务发现 / 日志 + common 头文件
add_library(chat_base INTERFACE)
target_include_directories(chat_base INTERFACE
    ${CHAT_SERVER_DIR}/common
    ${CHAT_SERVER_DIR}/third/include)
target_link_libraries(chat_base INTERFACE ${CHAT_BASE_LIBS} Threads::Threads)
add_library(chat::base ALIAS chat_base)

# chat::es / chat::redis / chat::mq  按需链接
foreach(group es redis mq)
    string(TOUPPER ${group} upper)
    add_library(chat_${group} INTERFACE)
    target_link_libraries(chat_${group} INTERFACE ${CHAT_${upper}_LIBS})
    add_library(chat::${group} ALIAS chat_${group})
endforeach()
# chat::proto / chat::odb 是真正编译出来的静态库, 分别定义在 proto/ 和 odb/ 目录

# ---------- 目标封装 ----------
# chat_add_server(<name> SOURCES <src...> [LIBS <lib...>])
#   声明一个子服务, 默认链接 chat::proto 和 chat::base, 并安装到 bin/
function(chat_add_server name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE chat::proto chat::base ${ARG_LIBS})
    install(TARGETS ${name} RUNTIME DESTINATION bin)
endfunction()

# chat_add_test_client(<name> SOURCES <src...> [LIBS <lib...>])
#   声明一个测试客户端, -DCHAT_BUILD_TESTS=OFF 时跳过 (生产构建不编译也不安装)
function(chat_add_test_client name)
    if(NOT CHAT_BUILD_TESTS)
        return()
    endif()
    chat_add_server(${name} ${ARGN})
endfunction()
