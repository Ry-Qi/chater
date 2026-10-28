# 所有 .proto 只生成、编译一次, 打成静态库 chat::proto 供各服务链接

set(proto_files
    base.proto channel.proto file.proto friend.proto gateway.proto
    message.proto notify.proto speech.proto transmite.proto user.proto)

set(proto_srcs "")
foreach(proto_file ${proto_files})
    get_filename_component(name ${proto_file} NAME_WE)
    set(proto_cc ${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.cc)
    set(proto_hh ${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.h)
    # 除 base 外都 import 了 base.proto, base 变化时也要重新生成
    add_custom_command(
        OUTPUT  ${proto_cc} ${proto_hh}
        COMMAND ${PROTOC_EXECUTABLE}
                --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                -I ${CMAKE_CURRENT_SOURCE_DIR}
                --experimental_allow_proto3_optional
                ${CMAKE_CURRENT_SOURCE_DIR}/${proto_file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${proto_file}
                ${CMAKE_CURRENT_SOURCE_DIR}/base.proto
        COMMENT "生成Protobuf框架代码: ${name}.pb.cc"
        VERBATIM)
    list(APPEND proto_srcs ${proto_cc})
endforeach()

add_library(chat_proto STATIC ${proto_srcs})
target_include_directories(chat_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(chat_proto PUBLIC protobuf)
add_library(chat::proto ALIAS chat_proto)
