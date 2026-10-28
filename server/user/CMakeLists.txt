# 用户管理子服务
chat_add_server(user_server
    SOURCES source/user_server.cc
    LIBS    chat::odb chat::es chat::redis)

chat_add_test_client(user_client
    SOURCES test/user_client.cc
    LIBS    gtest)
