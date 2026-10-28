# 群聊管理子服务
chat_add_server(channel_server
    SOURCES source/channel_server.cc
    LIBS    chat::odb chat::es)

chat_add_test_client(channel_client
    SOURCES test/channe_client.cc)
