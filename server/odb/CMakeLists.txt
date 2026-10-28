# 所有 ODB 实体只生成、编译一次, 打成静态库 chat::odb 供各服务链接

set(odb_files
    channel.hxx chat_session.hxx chat_session_member.hxx friend_apply.hxx
    message.hxx notify.hxx relation.hxx user.hxx)

# 实体头文件之间会互相 include (如 chat_session.hxx), 任一变化都重新生成
set(odb_headers "")
foreach(odb_file ${odb_files})
    list(APPEND odb_headers ${CMAKE_CURRENT_SOURCE_DIR}/${odb_file})
endforeach()

set(odb_srcs "")
foreach(odb_file ${odb_files})
    get_filename_component(name ${odb_file} NAME_WE)
    set(odb_prefix ${CMAKE_CURRENT_BINARY_DIR}/${name}-odb)
    add_custom_command(
        OUTPUT  ${odb_prefix}.cxx ${odb_prefix}.hxx ${odb_prefix}.ixx
        COMMAND ${ODB_EXECUTABLE}
                -d mysql --std c++17
                --generate-query --generate-schema
                --profile boost/date-time
                -o ${CMAKE_CURRENT_BINARY_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/${odb_file}
        DEPENDS ${odb_headers}
        COMMENT "生成ODB框架代码: ${name}-odb.cxx"
        VERBATIM)
    list(APPEND odb_srcs ${odb_prefix}.cxx)
endforeach()

add_library(chat_odb STATIC ${odb_srcs})
target_include_directories(chat_odb PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chat_odb PUBLIC chat::base ${CHAT_ODB_LIBS})
add_library(chat::odb ALIAS chat_odb)
