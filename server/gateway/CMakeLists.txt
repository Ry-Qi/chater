# 网关子服务 (HTTP + WebSocket)
chat_add_server(gateway_server
    SOURCES source/gateway_server.cc
    LIBS    chat::redis boost_system)

chat_add_test_client(gateway_client
    SOURCES test/gateway_client.cc
    LIBS    boost_system)
