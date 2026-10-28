# 消息存储子服务
chat_add_server(message_server
    SOURCES source/message_server.cc
    LIBS    chat::odb chat::es chat::mq)

chat_add_test_client(message_client
    SOURCES test/message_client.cc
    LIBS    gtest)
