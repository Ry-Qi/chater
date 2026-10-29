#include "base.pb.h"    // protobuf框架代码
#include "channel.hpp"  // 信道管理模块封装
#include "channel.pb.h" // protobuf框架代码
#include "connection.hpp"
#include "etcd.hpp"     // 服务注册模块封装
#include "file.pb.h"    // protobuf框架代码
#include "friend.pb.h"  // protobuf框架代码
#include "gateway.pb.h" // protobuf框架代码
#include "httplib.h"
#include "logger.hpp"   // 日志模块封装
#include "message.pb.h" // protobuf框架代码
#include "notify.pb.h"
#include "redis.hpp"      // redis数据管理客户端封装
#include "speech.pb.h"    // protobuf框架代码
#include "transmite.pb.h" // protobuf框架代码
#include "user.pb.h"      // protobuf框架代码
#include "utils.hpp"

namespace MY_IM {
#define GET_PHONE_VERIFY_CODE "/chat/user/get_phone_verify_code"
#define USERNAME_REGISTER "/chat/user/username_register"
#define USERNAME_LOGIN "/chat/user/username_login"
#define PHONE_REGISTER "/chat/user/phone_register"
#define PHONE_LOGIN "/chat/user/phone_login"
#define GET_USERINFO "/chat/user/get_user_info"
#define SET_USER_AVATAR "/chat/user/set_avatar"
#define SET_USER_NICKNAME "/chat/user/set_nickname"
#define SET_USER_DESC "/chat/user/set_description"
#define SET_USER_PHONE "/chat/user/set_phone"
#define FRIEND_GET_LIST "/chat/friend/get_friend_list"
#define FRIEND_APPLY "/chat/friend/add_friend_apply"
#define FRIEND_APPLY_PROCESS "/chat/friend/add_friend_process"
#define FRIEND_REMOVE "/chat/friend/remove_friend"
#define FRIEND_SEARCH "/chat/friend/search_friend"
#define FRIEND_GET_PENDING_EV "/chat/friend/get_pending_friend_events"
#define CSS_GET_LIST "/chat/friend/get_chat_session_list"
#define CSS_CREATE "/chat/friend/create_chat_session"
#define CSS_ADD_MEMBER "/chat/friend/add_chat_session_member"
#define CSS_GET_MEMBER "/chat/friend/get_chat_session_member"
#define MSG_GET_RANGE "/chat/message_storage/get_history"
#define MSG_GET_RECENT "/chat/message_storage/get_recent"
#define MSG_KEY_SEARCH "/chat/message_storage/search_history"
#define MSG_GET_SYNC "/chat/message_storage/get_sync"
#define NEW_MESSAGE "/chat/message_transmit/new_message"
#define FILE_GET_SINGLE "/chat/file/get_single_file"
#define FILE_GET_MULTI "/chat/file/get_multi_file"
#define FILE_PUT_SINGLE "/chat/file/put_single_file"
#define FILE_PUT_MULTI "/chat/file/put_multi_file"
#define SPEECH_RECOGNITION "/chat/speech/recognition"
#define CHANNEL_CREATE "/chat/channel/create"
#define CHANNEL_NEW_MESSAGE "/chat/channel/new_message"
class GatewayServer {
public:
  using ptr = std::shared_ptr<GatewayServer>;
  GatewayServer(int websocket_port, int http_port,
                const std::shared_ptr<sw::redis::Redis> &redis_client,
                const ServiceManager::ptr &channels,
                const Discoverer::ptr &service_discoverer,
                const std::string user_service_name,
                const std::string file_service_name,
                const std::string speech_service_name,
                const std::string message_service_name,
                const std::string transmite_service_name,
                const std::string friend_service_name,
                const std::string channel_service_name)
      : _redis_session(std::make_shared<SessionClient>(redis_client)),
        _redis_status(std::make_shared<StatusClient>(redis_client)),
        _mm_channels(channels), _service_discoverer(service_discoverer),
        _user_service_name(user_service_name),
        _file_service_name(file_service_name),
        _speech_service_name(speech_service_name),
        _message_service_name(message_service_name),
        _transmite_service_name(transmite_service_name),
        _friend_service_name(friend_service_name),
        _channel_service_name(channel_service_name),
        _connections(std::make_shared<Connection>()) {

    _ws_server.set_access_channels(websocketpp::log::alevel::none);
    _ws_server.init_asio();
    _ws_server.set_open_handler(
        std::bind(&GatewayServer::onOpen, this, std::placeholders::_1));
    _ws_server.set_close_handler(
        std::bind(&GatewayServer::onClose, this, std::placeholders::_1));
    auto wscb = std::bind(&GatewayServer::onMessage, this,
                          std::placeholders::_1, std::placeholders::_2);
    _ws_server.set_message_handler(wscb);
    _ws_server.set_reuse_addr(true);
    _ws_server.listen(websocket_port);
    _ws_server.start_accept();

    // _http_server.Post(GET_PHONE_VERIFY_CODE  ,
    // (httplib::Server::Handler)std::bind(&GatewayServer::GetPhoneVerifyCode ,
    // this, std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(USERNAME_REGISTER,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::UserRegister, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(USERNAME_LOGIN,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::UserLogin, this,
                          std::placeholders::_1, std::placeholders::_2));
    // _http_server.Post(PHONE_REGISTER         ,
    // (httplib::Server::Handler)std::bind(&GatewayServer::PhoneRegister , this,
    // std::placeholders::_1, std::placeholders::_2));
    // _http_server.Post(PHONE_LOGIN            ,
    // (httplib::Server::Handler)std::bind(&GatewayServer::PhoneLogin , this,
    // std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(GET_USERINFO,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetUserInfo, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(SET_USER_AVATAR,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SetUserAvatar, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(SET_USER_NICKNAME,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SetUserNickname, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(SET_USER_DESC,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SetUserDescription, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(SET_USER_PHONE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SetUserPhoneNumber, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_GET_LIST,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetFriendList, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_APPLY,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::FriendAdd, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_APPLY_PROCESS,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::FriendAddProcess, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_REMOVE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::FriendRemove, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_SEARCH,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::FriendSearch, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FRIEND_GET_PENDING_EV,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetPendingFriendEventList, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CSS_GET_LIST,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetChatSessionList, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CSS_CREATE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::ChatSessionCreate, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CSS_ADD_MEMBER,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::AddChatSessionMember, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CSS_GET_MEMBER,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetChatSessionMember, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_GET_RANGE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetHistoryMsg, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_GET_RECENT,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetRecentMsg, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_GET_RECENT,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetRecentMsg, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_KEY_SEARCH,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::MsgSearch, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_GET_SYNC,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetSyncMsg, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(MSG_GET_SYNC,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetSyncMsg, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(NEW_MESSAGE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::NewMessage, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FILE_GET_SINGLE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetSingleFile, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FILE_GET_MULTI,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::GetMultiFile, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FILE_PUT_SINGLE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::PutSingleFile, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(FILE_PUT_MULTI,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::PutMultiFile, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(SPEECH_RECOGNITION,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SpeechRecognition, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CHANNEL_CREATE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::CreateChannel, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_server.Post(CHANNEL_NEW_MESSAGE,
                      (httplib::Server::Handler)std::bind(
                          &GatewayServer::SendChannelMessage, this,
                          std::placeholders::_1, std::placeholders::_2));
    _http_thread = std::thread(
        [this, http_port]() { _http_server.listen("0.0.0.0", http_port); });

    _http_thread.detach();
  }
  void start() { _ws_server.run(); }

private:
  void onOpen(websocketpp::connection_hdl hdl) {
    LOG_DEBUG("websocket长连接建立成功 {}",
              (size_t)_ws_server.get_con_from_hdl(hdl).get());
  }
  void onClose(websocketpp::connection_hdl hdl) {
    // 长连接断开时做的清理工作
    // 0. 通过连接对象，获取对应的用户ID与登录会话ID
    auto conn = _ws_server.get_con_from_hdl(hdl);
    std::string uid, ssid;
    bool ret = _connections->client(conn, uid, ssid);
    if (ret == false) {
      LOG_WARNING("长连接断开，未找到长连接对应的客户端信息！");
      return;
    }
    // 1. 移除登录状态信息（支持按设备维度）
    auto device_type = _redis_session->device_type(ssid);
    if (device_type != "") {
      _redis_status->remove(uid + "|" + *device_type);
    } else {
      _redis_status->remove(uid);
    }
    // 2. 移除登录会话信息
    _redis_session->remove(ssid);
    // 3. 移除长连接管理数据
    _connections->remove(conn);
    LOG_DEBUG("{} {} {} 长连接断开，清理缓存数据!", ssid, uid,
              (size_t)conn.get());
  }
  void keepAlive(server_t::connection_ptr conn) {
    if (!conn ||
        conn->get_state() != websocketpp::session::state::value::open) {
      LOG_DEBUG("非正常连接状态，结束连接保活");
      return;
    }
    conn->ping("I am here.");
    _ws_server.set_timer(60000,
                         std::bind(&GatewayServer::keepAlive, this, conn));
  }
  void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
    auto conn = _ws_server.get_con_from_hdl(hdl);
    std::string uid, ssid;
    // 离线用户上线后
    // 网关会给其推送离线消息，客户端收到后需要进行ACK确认，确认后网关会删除对应的离线消息
    if (_connections->client(conn, uid, ssid)) {
      NotifyAckReq ack_req;
      bool ack_ok = ack_req.ParseFromString(msg->get_payload());
      if (!ack_ok) {
        LOG_WARNING("收到未识别的 websocket 消息，已忽略");
        return;
      }
      if (ack_req.notify_event_id_list_size() == 0) {
        return;
      }
      std::string rid =
          ack_req.request_id().empty() ? Uuid() : ack_req.request_id();
      AckOfflineNotifyReq req;
      AckOfflineNotifyRsp rsp;
      req.set_request_id(rid);
      req.set_user_id(uid);
      for (int i = 0; i < ack_req.notify_event_id_list_size(); i++) {
        req.add_notify_event_id_list(ack_req.notify_event_id_list(i));
      }
      auto channel = _mm_channels->GetChannel(_message_service_name);
      if (!channel) {
        LOG_ERROR("{} 未找到消息子服务节点，ACK处理失败", rid);
        return;
      }
      MY_IM::MsgStorageService_Stub stub(channel.get());
      brpc::Controller cntl;
      stub.AckOfflineNotify(&cntl, &req, &rsp, nullptr);
      if (cntl.Failed() || !rsp.success()) {
        LOG_ERROR("{} ACK离线通知失败: {}", rid,
                  cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
        return;
      }
      LOG_DEBUG("{} ACK离线通知成功，数量: {}", rid,
                ack_req.notify_event_id_list_size());
      return;
    }

    // 未认证连接：首条消息按鉴权消息处理
    ClientAuthenticationReq request;
    bool ret = request.ParseFromString(msg->get_payload());
    if (ret == false) {
      LOG_ERROR("长连接身份识别失败：正文反序列化失败！");
      _ws_server.close(hdl, websocketpp::close::status::unsupported_data,
                       "正文反序列化失败!");
      return;
    }
    // 3. 在会话信息缓存中，查找会话信息
    ssid = request.session_id();
    auto optionalUid = _redis_session->uid(ssid);
    // 4. 会话信息不存在则关闭连接
    if (!optionalUid) {
      LOG_ERROR("长连接身份识别失败：未找到会话信息 {}！", ssid);
      _ws_server.close(hdl, websocketpp::close::status::unsupported_data,
                       "未找到会话信息!");
      return;
    }
    // 5. 会话信息存在，则添加长连接管理
    _connections->insert(conn, *optionalUid, ssid);
    LOG_DEBUG("新增长连接管理：{}-{}-{}", ssid, *optionalUid,
              (size_t)conn.get());
    _ReplayOfflineNotify(Uuid(), *optionalUid, conn);
    _NotifySyncToCurrentConnection(*optionalUid, ssid, conn);
    keepAlive(conn);
  }
  void UserRegister(const httplib::Request &request,
                    httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    UserRegisterReq req;
    UserRegisterRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户名注册请求正文反序列化失败！");
      return err_response("用户名注册请求正文反序列化失败！");
    }
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.UserRegister(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }

    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void UserLogin(const httplib::Request &request, httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    UserLoginReq req;
    UserLoginRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户登录请求正文反序列化失败！");
      return err_response("用户登录请求正文反序列化失败！");
    }
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.UserLogin(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetUserInfo(const httplib::Request &request,
                   httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    GetUserInfoReq req;
    GetUserInfoRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取用户信息请求正文反序列化失败！");
      return err_response("获取用户信息请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetUserInfo(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SetUserAvatar(const httplib::Request &request,
                     httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    SetUserAvatarReq req;
    SetUserAvatarRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户头像设置请求正文反序列化失败！");
      return err_response("用户头像设置请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SetUserAvatar(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SetUserNickname(const httplib::Request &request,
                       httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    SetUserNicknameReq req;
    SetUserNicknameRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户昵称设置请求正文反序列化失败！");
      return err_response("用户昵称设置请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SetUserNickname(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SetUserDescription(const httplib::Request &request,
                          httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    SetUserDescriptionReq req;
    SetUserDescriptionRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户签名设置请求正文反序列化失败！");
      return err_response("用户签名设置请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SetUserDescription(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SetUserPhoneNumber(const httplib::Request &request,
                          httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    SetUserPhoneNumberReq req;
    SetUserPhoneNumberRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户手机号设置请求正文反序列化失败！");
      return err_response("用户手机号设置请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SetUserPhoneNumber(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return err_response("用户子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetFriendList(const httplib::Request &request,
                     httplib::Response &response) {
    // 1. 取出http请求正文，将正文进行反序列化
    GetFriendListReq req;
    GetFriendListRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取好友列表请求正文反序列化失败！");
      return err_response("获取好友列表请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 2. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetFriendList(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 3. 得到用户子服务的响应后，将响应内容进行序列化作为http响应正文
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }

  std::shared_ptr<GetUserInfoRsp> _GetUserInfo(const std::string &rid,
                                               const std::string &uid) {
    GetUserInfoReq req;
    auto rsp = std::make_shared<GetUserInfoRsp>();
    req.set_request_id(rid);
    req.set_user_id(uid);
    // 2. 将请求转发给用户子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_user_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return std::shared_ptr<GetUserInfoRsp>();
    }
    MY_IM::UserService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetUserInfo(&cntl, &req, rsp.get(), nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 用户子服务调用失败！", req.request_id());
      return std::shared_ptr<GetUserInfoRsp>();
    }
    return rsp;
  }
  void FriendAdd(const httplib::Request &request, httplib::Response &response) {
    // 好友申请的业务处理中，好友子服务其实只是在数据库创建了申请事件
    // 网关需要做的事情：当好友子服务将业务处理完毕后，如果处理是成功的--需要通知被申请方
    // 1. 正文的反序列化，提取关键要素：登录会话ID
    FriendAddReq req;
    FriendAddRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("申请好友请求正文反序列化失败！");
      return err_response("申请好友请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.FriendAdd(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 4. 若业务处理成功 ---
    // 且获取被申请方长连接成功，则向被申请放进行好友申请事件通知
    if (rsp.success()) {
      auto user_rsp = _GetUserInfo(req.request_id(), *uid);
      if (!user_rsp) {
        LOG_ERROR("{} 获取当前客户端用户信息失败！", req.request_id());
        return err_response("获取当前客户端用户信息失败！");
      }
      NotifyMessage notify;
      notify.set_notify_type(NotifyType::FRIEND_ADD_APPLY_NOTIFY);
      notify.mutable_friend_add_apply()->mutable_user_info()->CopyFrom(
          user_rsp->user_info());
      _PushNotifyOrStore(req.request_id(), req.respondent_id(), notify);
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void FriendAddProcess(const httplib::Request &request,
                        httplib::Response &response) {
    // 好友申请的处理-----
    FriendAddProcessReq req;
    FriendAddProcessRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("好友申请处理请求正文反序列化失败！");
      return err_response("好友申请处理请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.FriendAddProcess(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }

    if (rsp.success()) {
      auto process_user_rsp = _GetUserInfo(req.request_id(), *uid);
      if (!process_user_rsp) {
        LOG_ERROR("{} 获取用户信息失败！", req.request_id());
        return err_response("获取用户信息失败！");
      }
      auto apply_user_rsp = _GetUserInfo(req.request_id(), req.apply_user_id());
      if (!apply_user_rsp) {
        LOG_ERROR("{} 获取用户信息失败！", req.request_id());
        return err_response("获取用户信息失败！");
      }
      // 4. 将处理结果给申请人进行通知
      NotifyMessage notify;
      notify.set_notify_type(NotifyType::FRIEND_ADD_PROCESS_NOTIFY);
      auto process_result = notify.mutable_friend_process_result();
      process_result->mutable_user_info()->CopyFrom(
          process_user_rsp->user_info());
      process_result->set_agree(req.agree());
      _PushNotifyOrStore(req.request_id(), req.apply_user_id(), notify);
      LOG_DEBUG("对申请人进行申请处理结果通知！");
      // 5. 若处理结果是同意 --- 会伴随着单聊会话的创建 --
      // 因此需要对双方进行会话创建的通知
      if (req.agree()) { // 对申请人的通知---会话信息就是处理人信息
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_SESSION_CREATE_NOTIFY);
        auto chat_session = notify.mutable_new_chat_session_info();
        chat_session->mutable_chat_session_info()->set_single_chat_friend_id(
            *uid);
        chat_session->mutable_chat_session_info()->set_chat_session_id(
            rsp.new_session_id());
        chat_session->mutable_chat_session_info()->set_chat_session_name(
            process_user_rsp->user_info().nickname());
        chat_session->mutable_chat_session_info()->set_avatar(
            process_user_rsp->user_info().avatar());
        _PushNotifyOrStore(req.request_id(), req.apply_user_id(), notify);
        LOG_DEBUG("对申请人进行会话创建通知！");
      }
      if (req.agree()) { // 对处理人的通知 --- 会话信息就是申请人信息
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_SESSION_CREATE_NOTIFY);
        auto chat_session = notify.mutable_new_chat_session_info();
        chat_session->mutable_chat_session_info()->set_single_chat_friend_id(
            req.apply_user_id());
        chat_session->mutable_chat_session_info()->set_chat_session_id(
            rsp.new_session_id());
        chat_session->mutable_chat_session_info()->set_chat_session_name(
            apply_user_rsp->user_info().nickname());
        chat_session->mutable_chat_session_info()->set_avatar(
            apply_user_rsp->user_info().avatar());
        _PushNotifyOrStore(req.request_id(), *uid, notify);
        LOG_DEBUG("对处理人进行会话创建通知！");
      }
    }
    // 6. 对客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void FriendRemove(const httplib::Request &request,
                    httplib::Response &response) {
    // 1. 正文的反序列化，提取关键要素：登录会话ID
    FriendRemoveReq req;
    FriendRemoveRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("删除好友请求正文反序列化失败！");
      return err_response("删除好友请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.FriendRemove(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 4. 若业务处理成功 ---
    // 且获取被申请方长连接成功，则向被申请放进行好友申请事件通知
    if (rsp.success()) {
      LOG_ERROR("对被删除人 {} 进行好友删除通知！", req.peer_id());
      NotifyMessage notify;
      notify.set_notify_type(NotifyType::FRIEND_REMOVE_NOTIFY);
      notify.mutable_friend_remove()->set_user_id(*uid);
      _PushNotifyOrStore(req.request_id(), req.peer_id(), notify);
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void FriendSearch(const httplib::Request &request,
                    httplib::Response &response) {
    FriendSearchReq req;
    FriendSearchRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("用户搜索请求正文反序列化失败！");
      return err_response("用户搜索请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.FriendSearch(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetPendingFriendEventList(const httplib::Request &request,
                                 httplib::Response &response) {
    GetPendingFriendEventListReq req;
    GetPendingFriendEventListRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取待处理好友申请请求正文反序列化失败！");
      return err_response("获取待处理好友申请请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetPendingFriendEventList(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetChatSessionList(const httplib::Request &request,
                          httplib::Response &response) {
    GetChatSessionListReq req;
    GetChatSessionListRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取聊天会话列表请求正文反序列化失败！");
      return err_response("获取聊天会话列表请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetChatSessionList(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetChatSessionMember(const httplib::Request &request,
                            httplib::Response &response) {
    GetChatSessionMemberReq req;
    GetChatSessionMemberRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取聊天会话成员请求正文反序列化失败！");
      return err_response("获取聊天会话成员请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetChatSessionMember(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void ChatSessionCreate(const httplib::Request &request,
                         httplib::Response &response) {
    ChatSessionCreateReq req;
    ChatSessionCreateRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("创建聊天会话请求正文反序列化失败！");
      return err_response("创建聊天会话请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.ChatSessionCreate(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }
    // 4. 若业务处理成功 ---
    // 且获取被申请方长连接成功，则向被申请放进行好友申请事件通知
    if (rsp.success()) {
      for (int i = 0; i < req.member_id_list_size(); i++) {
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_SESSION_CREATE_NOTIFY);
        auto chat_session = notify.mutable_new_chat_session_info();
        chat_session->mutable_chat_session_info()->CopyFrom(
            rsp.chat_session_info());
        _PushNotifyOrStore(req.request_id(), req.member_id_list(i), notify);
        LOG_DEBUG("对群聊成员 {} 进行会话创建通知", req.member_id_list(i));
      }
    }
    // 5. 向客户端进行响应
    rsp.clear_chat_session_info();
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void AddChatSessionMember(const httplib::Request &request,
                            httplib::Response &response) {
    AddChatSessionMemberReq req;
    AddChatSessionMemberRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("群聊添加成员请求正文反序列化失败！");
      return err_response("群聊添加成员请求正文反序列化失败！");
    }
    // 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 转发给好友子服务
    auto channel = _mm_channels->GetChannel(_friend_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FriendService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.AddChatSessionMember(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 好友子服务调用失败！", req.request_id());
      return err_response("好友子服务调用失败！");
    }

    // 对“实际新增成功”的被邀请人进行会话通知
    if (rsp.success()) {
      for (int i = 0; i < rsp.added_member_id_list_size(); i++) {
        const auto &invited_uid = rsp.added_member_id_list(i);
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_SESSION_CREATE_NOTIFY);
        auto chat_session = notify.mutable_new_chat_session_info();
        chat_session->mutable_chat_session_info()->CopyFrom(
            rsp.chat_session_info());
        _PushNotifyOrStore(req.request_id(), invited_uid, notify);
        LOG_DEBUG("对新增群聊成员 {} 进行会话创建通知", invited_uid);
      }
    }
    rsp.clear_chat_session_info();
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void CreateChannel(const httplib::Request &request,
                     httplib::Response &response) {
    CreateChannelReq req;
    CreateChannelRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("创建频道请求正文反序列化失败！");
      return err_response("创建频道请求正文反序列化失败！");
    }
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    auto channel = _mm_channels->GetChannel(_channel_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的频道子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的频道子服务节点！");
    }
    MY_IM::ChannelService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.CreateChannel(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 频道子服务调用失败！", req.request_id());
      return err_response("频道子服务调用失败！");
    }
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SendChannelMessage(const httplib::Request &request,
                          httplib::Response &response) {
    SendChannelMessageReq req;
    SendChannelMessageRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("频道消息请求正文反序列化失败！");
      return err_response("频道消息请求正文反序列化失败！");
    }
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    auto channel = _mm_channels->GetChannel(_channel_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的频道子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的频道子服务节点！");
    }
    MY_IM::ChannelService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SendChannelMessage(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 频道子服务调用失败！", req.request_id());
      return err_response("频道子服务调用失败！");
    }
    if (rsp.success()) {
      for (int i = 0; i < rsp.target_id_list_size(); i++) {
        std::string notify_uid = rsp.target_id_list(i);
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_MESSAGE_NOTIFY);
        auto msg_info = notify.mutable_new_message_info();
        msg_info->mutable_message_info()->CopyFrom(rsp.message());
        // 目标通知 uid 是自己：当前设备已有该消息，直接推给其它在线设备
        if (notify_uid == *uid) {
          _PushNotifyToOtherDevices(*uid, ssid, notify);
          continue;
        }
        _PushNotifyOrStore(req.request_id(), notify_uid, notify);
      }
    }
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetHistoryMsg(const httplib::Request &request,
                     httplib::Response &response) {
    GetHistoryMsgReq req;
    GetHistoryMsgRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取区间消息请求正文反序列化失败！");
      return err_response("获取区间消息请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetHistoryMsg(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 消息存储子服务调用失败！", req.request_id());
      return err_response("消息存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetRecentMsg(const httplib::Request &request,
                    httplib::Response &response) {
    GetRecentMsgReq req;
    GetRecentMsgRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("获取最近消息请求正文反序列化失败！");
      return err_response("获取最近消息请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetRecentMsg(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 消息存储子服务调用失败！", req.request_id());
      return err_response("消息存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void MsgSearch(const httplib::Request &request, httplib::Response &response) {
    MsgSearchReq req;
    MsgSearchRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("消息搜索请求正文反序列化失败！");
      return err_response("消息搜索请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.MsgSearch(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 消息存储子服务调用失败！", req.request_id());
      return err_response("消息存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetSyncMsg(const httplib::Request &request,
                  httplib::Response &response) {
    GetSyncMsgReq req;
    GetSyncMsgRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("消息同步请求正文反序列化失败！");
      return err_response("消息同步请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给消息存储子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetSyncMsg(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 消息存储子服务调用失败！", req.request_id());
      return err_response("消息存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetSingleFile(const httplib::Request &request,
                     httplib::Response &response) {
    GetSingleFileReq req;
    GetSingleFileRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("单文件下载请求正文反序列化失败！");
      return err_response("单文件下载请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_file_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FileService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetSingleFile(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 文件存储子服务调用失败！", req.request_id());
      return err_response("文件存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void GetMultiFile(const httplib::Request &request,
                    httplib::Response &response) {
    GetMultiFileReq req;
    GetMultiFileRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("单文件下载请求正文反序列化失败！");
      return err_response("单文件下载请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_file_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FileService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetMultiFile(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 文件存储子服务调用失败！", req.request_id());
      return err_response("文件存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void PutSingleFile(const httplib::Request &request,
                     httplib::Response &response) {
    PutSingleFileReq req;
    PutSingleFileRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("单文件上传请求正文反序列化失败！");
      return err_response("单文件上传请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_file_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FileService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.PutSingleFile(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 文件存储子服务调用失败！", req.request_id());
      return err_response("文件存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void PutMultiFile(const httplib::Request &request,
                    httplib::Response &response) {
    PutMultiFileReq req;
    PutMultiFileRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("批量文件上传请求正文反序列化失败！");
      return err_response("批量文件上传请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_file_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::FileService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.PutMultiFile(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 文件存储子服务调用失败！", req.request_id());
      return err_response("文件存储子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }
  void SpeechRecognition(const httplib::Request &request,
                         httplib::Response &response) {
    LOG_DEBUG("收到语音转文字请求！");
    SpeechRecognitionReq req;
    SpeechRecognitionRsp rsp;
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("语音识别请求正文反序列化失败！");
      return err_response("语音识别请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_speech_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::SpeechService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.SpeechRecognition(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 语音识别子服务调用失败！", req.request_id());
      return err_response("语音识别子服务调用失败！");
    }
    // 5. 向客户端进行响应
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }

  void NewMessage(const httplib::Request &request,
                  httplib::Response &response) {
    NewMessageReq req;               // 转发子服务的请求
    NewMessageRsp rsp;               // 给客户端的响应
    GetTransmitTargetRsp target_rsp; // 转发子服务的响应
    auto err_response = [&req, &rsp,
                         &response](const std::string &errmsg) -> void {
      rsp.set_success(false);
      rsp.set_errmsg(errmsg);
      response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
    };
    bool ret = req.ParseFromString(request.body);
    if (ret == false) {
      LOG_ERROR("新消息请求正文反序列化失败！");
      return err_response("新消息请求正文反序列化失败！");
    }
    // 2. 客户端身份识别与鉴权
    std::string ssid = req.session_id();
    auto uid = _redis_session->uid(ssid);
    if (!uid) {
      LOG_ERROR("{} 获取登录会话关联用户信息失败！", ssid);
      return err_response("获取登录会话关联用户信息失败！");
    }
    req.set_user_id(*uid);
    // 3. 将请求转发给好友子服务进行业务处理
    auto channel = _mm_channels->GetChannel(_transmite_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到可提供业务处理的用户子服务节点！", req.request_id());
      return err_response("未找到可提供业务处理的用户子服务节点！");
    }
    MY_IM::MsgTransmitService_Stub stub(channel.get());
    brpc::Controller cntl;
    // 让转发子服务组装消息、查会话成员、并发布到 MQ（用于持久化）。
    stub.GetTransmitTarget(&cntl, &req, &target_rsp, nullptr);
    if (cntl.Failed()) {
      LOG_ERROR("{} 消息转发子服务调用失败！", req.request_id());
      return err_response("消息转发子服务调用失败！");
    }

    // 4. 若业务处理成功 ---
    // 且获取被申请方长连接成功，则向被申请方进行通知
    if (target_rsp.success()) {
      for (int i = 0; i < target_rsp.target_id_list_size(); i++) {
        std::string notify_uid = target_rsp.target_id_list(i);
        NotifyMessage notify;
        notify.set_notify_type(NotifyType::CHAT_MESSAGE_NOTIFY);
        auto msg_info = notify.mutable_new_message_info();
        msg_info->mutable_message_info()->CopyFrom(target_rsp.message());
        // 目标通知 uid 是自己：当前设备已有该消息，直接推给其它在线设备
        if (notify_uid == *uid) {
          _PushNotifyToOtherDevices(*uid, ssid, notify);
          continue;
        }
        _PushNotifyOrStore(req.request_id(), notify_uid, notify);
      }
    }
    // 5. 向客户端进行响应
    rsp.set_request_id(req.request_id());
    rsp.set_success(target_rsp.success());
    rsp.set_errmsg(target_rsp.errmsg());
    response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
  }

private:
  bool _PushNotifyOrStore(const std::string &rid, const std::string &target_uid,
                          NotifyMessage &notify) {
    if (target_uid.empty())
      return false;
    if (notify.notify_event_id().empty()) {
      notify.set_notify_event_id(Uuid());
    }
    const auto payload = notify.SerializeAsString();
    auto conns = _connections->connections(target_uid);
    // 如果目标用户有在线设备，则直接发送通知
    if (!conns.empty()) {
      for (auto &conn : conns) {
        conn->send(payload, websocketpp::frame::opcode::value::binary);
      }
      return true;
    }
    // 如果目标用户没有在线设备，则将通知存储到数据库
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到消息子服务节点，离线通知存储失败", rid);
      return false;
    }
    CreateOfflineNotifyReq req;
    CreateOfflineNotifyRsp rsp;
    req.set_request_id(rid);
    req.set_user_id(target_uid);
    req.set_notify_type(static_cast<int32_t>(notify.notify_type()));
    req.set_notify_event_id(notify.notify_event_id());
    req.set_payload(payload);
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.CreateOfflineNotify(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed() || !rsp.success()) {
      LOG_ERROR("{} 离线通知入库失败: {}", rid,
                cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
      return false;
    }
    return true;
  }

  void _ReplayOfflineNotify(const std::string &rid, const std::string &uid,
                            const server_t::connection_ptr &conn) {
    auto channel = _mm_channels->GetChannel(_message_service_name);
    if (!channel) {
      LOG_ERROR("{} 未找到消息子服务节点，离线通知回放失败", rid);
      return;
    }
    GetOfflineNotifyReq req;
    GetOfflineNotifyRsp rsp;
    req.set_request_id(rid);
    req.set_user_id(uid);
    req.set_limit(100);
    MY_IM::MsgStorageService_Stub stub(channel.get());
    brpc::Controller cntl;
    stub.GetOfflineNotify(&cntl, &req, &rsp, nullptr);
    if (cntl.Failed() || !rsp.success()) {
      LOG_ERROR("{} 拉取离线通知失败: {}", rid,
                cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
      return;
    }
    for (int i = 0; i < rsp.notify_list_size(); i++) {
      conn->send(rsp.notify_list(i).payload(),
                 websocketpp::frame::opcode::value::binary);
    }
    if (rsp.notify_list_size() > 0) {
      LOG_DEBUG("{} 回放离线通知数量: {}", rid, rsp.notify_list_size());
    }
  }

  // 当前设备完成鉴权后，主动提示客户端拉取一次增量消息。
  void _NotifySyncToCurrentConnection(const std::string &uid,
                                      const std::string &ssid,
                                      const server_t::connection_ptr &conn) {
    if (!conn)
      return;

    NotifyMessage notify;
    notify.set_notify_type(NotifyType::NOTIFY_SYNC);
    notify.mutable_notify_sync();
    auto payload = notify.SerializeAsString();
    conn->send(payload, websocketpp::frame::opcode::value::binary);
    LOG_DEBUG("{} {} 当前设备已发送 notify_sync", uid, ssid);
  }

  // 向同一 uid 的其它在线设备直接推送通知（跳过当前设备，不做离线存储）
  // 离线设备在重连鉴权后会收到 notify_sync 并拉取增量，无需在此入库
  void _PushNotifyToOtherDevices(const std::string &uid,
                                 const std::string &self_ssid,
                                 NotifyMessage &notify) {
    auto conns = _connections->connections(uid);
    if (conns.empty())
      return;
    if (notify.notify_event_id().empty()) {
      notify.set_notify_event_id(Uuid());
    }
    auto payload = notify.SerializeAsString();

    for (auto &conn : conns) {
      std::string c_uid, c_ssid;
      if (!_connections->client(conn, c_uid, c_ssid))
        continue;
      if (c_uid != uid)
        continue;
      if (c_ssid == self_ssid)
        continue; // 跳过当前设备
      conn->send(payload, websocketpp::frame::opcode::value::binary);
    }
  }

  SessionClient::ptr _redis_session;
  StatusClient::ptr _redis_status;

  std::string _user_service_name;
  std::string _file_service_name;
  std::string _speech_service_name;
  std::string _message_service_name;
  std::string _transmite_service_name;
  std::string _friend_service_name;
  std::string _channel_service_name;
  ServiceManager::ptr _mm_channels;
  Discoverer::ptr _service_discoverer;

  Connection::ptr _connections;

  server_t _ws_server;
  httplib::Server _http_server;
  std::thread _http_thread;
};

class GatewayServerBuilder {
public:
  // 构造redis客户端对象
  void make_redis_object(const std::string &host, int port, int db,
                         bool keep_alive) {
    _redis_client = RedisFactory::create(host, port, db, keep_alive);
  }
  // 用于构造服务发现客户端&信道管理对象
  void make_discoverer_object(const std::string &reg_host,
                              const std::string &base_service_name,
                              const std::string &file_service_name,
                              const std::string &speech_service_name,
                              const std::string &message_service_name,
                              const std::string &friend_service_name,
                              const std::string &user_service_name,
                              const std::string &transmite_service_name,
                              const std::string &channel_service_name) {
    _file_service_name = file_service_name;
    _speech_service_name = speech_service_name;
    _message_service_name = message_service_name;
    _friend_service_name = friend_service_name;
    _user_service_name = user_service_name;
    _transmite_service_name = transmite_service_name;
    _channel_service_name = channel_service_name;
    _mm_channels = std::make_shared<ServiceManager>();
    _mm_channels->FollowOn(file_service_name);
    _mm_channels->FollowOn(speech_service_name);
    _mm_channels->FollowOn(message_service_name);
    _mm_channels->FollowOn(friend_service_name);
    _mm_channels->FollowOn(user_service_name);
    _mm_channels->FollowOn(transmite_service_name);
    _mm_channels->FollowOn(channel_service_name);
    auto put_cb = std::bind(&ServiceManager::OnlineCall, _mm_channels.get(),
                            std::placeholders::_1, std::placeholders::_2);
    auto del_cb = std::bind(&ServiceManager::OfflineCall, _mm_channels.get(),
                            std::placeholders::_1, std::placeholders::_2);
    _service_discoverer = std::make_shared<Discoverer>(
        reg_host, base_service_name, put_cb, del_cb);
  }
  void make_server_object(int websocket_port, int http_port) {
    _websocket_port = websocket_port;
    _http_port = http_port;
  }
  // 构造RPC服务器对象
  GatewayServer::ptr build() {
    if (!_redis_client) {
      LOG_ERROR("还未初始化Redis客户端模块！");
      abort();
    }
    if (!_service_discoverer) {
      LOG_ERROR("还未初始化服务发现模块！");
      abort();
    }
    if (!_mm_channels) {
      LOG_ERROR("还未初始化信道管理模块！");
      abort();
    }
    GatewayServer::ptr server = std::make_shared<GatewayServer>(
        _websocket_port, _http_port, _redis_client, _mm_channels,
        _service_discoverer, _user_service_name, _file_service_name,
        _speech_service_name, _message_service_name, _transmite_service_name,
        _friend_service_name, _channel_service_name);
    return server;
  }

private:
  int _websocket_port;
  int _http_port;

  std::shared_ptr<sw::redis::Redis> _redis_client;

  std::string _file_service_name;
  std::string _speech_service_name;
  std::string _message_service_name;
  std::string _friend_service_name;
  std::string _user_service_name;
  std::string _transmite_service_name;
  std::string _channel_service_name;
  ServiceManager::ptr _mm_channels;
  Discoverer::ptr _service_discoverer;
};
} // namespace MY_IM
