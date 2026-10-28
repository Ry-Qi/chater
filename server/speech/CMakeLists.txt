# 语音识别子服务
chat_add_server(speech_server
    SOURCES source/speech_server.cc)

chat_add_test_client(speech_client
    SOURCES test/speech_client.cc)
