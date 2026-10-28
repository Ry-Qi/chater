# 好友管理子服务
chat_add_server(friend_server
    SOURCES source/friend_server.cc
    LIBS    chat::odb chat::es)

chat_add_test_client(friend_client
    SOURCES test/friend_client.cc
    LIBS    gtest)
