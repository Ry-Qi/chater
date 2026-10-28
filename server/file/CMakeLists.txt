# 文件存储子服务
chat_add_server(file_server
    SOURCES source/file_server.cc)

chat_add_test_client(file_client
    SOURCES test/file_client.cc
    LIBS    gtest)
