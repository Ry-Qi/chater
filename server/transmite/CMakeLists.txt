# 消息转发子服务
chat_add_server(transmite_server
    SOURCES source/transmite_server.cc
    LIBS    chat::odb chat::mq chat::redis)

chat_add_test_client(transmite_client
    SOURCES test/transmite_client.cc
    LIBS    gtest)
