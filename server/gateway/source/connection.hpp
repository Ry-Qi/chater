#include "logger.hpp"
#include <unordered_set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace MY_IM {
typedef websocketpp::server<websocketpp::config::asio> server_t;
// 连接的类型： server_t::connection_ptr

class Connection {
public:
  struct Client {
    Client(const std::string &u, const std::string &s) : uid(u), ssid(s) {}
    std::string uid;
    std::string ssid;
  };
  using ptr = std::shared_ptr<Connection>;
  Connection() {}
  ~Connection() {}
  void insert(const server_t::connection_ptr &conn, const std::string &uid,
              const std::string &ssid) {
    std::unique_lock<std::mutex> lock(_mutex);
    _uid_sessions[uid].insert(ssid);
    _ssid_connections[ssid] = conn;
    _conn_clients.insert(std::make_pair(conn, Client(uid, ssid)));
    LOG_DEBUG("新增长连接用户信息：{}-{}-{}", (size_t)conn.get(), uid, ssid);
  }
  std::vector<server_t::connection_ptr> connections(const std::string &uid) {
    std::unique_lock<std::mutex> lock(_mutex);
    auto it = _uid_sessions.find(uid);
    if (it == _uid_sessions.end() || it->second.empty()) {
      LOG_ERROR("未找到 {} 客户端的长连接！", uid);
      return {};
    }
    std::vector<server_t::connection_ptr> result;
    result.reserve(it->second.size());
    for (const auto &ssid : it->second) {
      auto sit = _ssid_connections.find(ssid);
      if (sit != _ssid_connections.end()) {
        result.push_back(sit->second);
      }
    }
    LOG_DEBUG("找到 {} 客户端的长连接数量：{}", uid, result.size());
    return result;
  }
  bool client(const server_t::connection_ptr &conn, std::string &uid,
              std::string &ssid) {
    std::unique_lock<std::mutex> lock(_mutex);
    auto it = _conn_clients.find(conn);
    if (it == _conn_clients.end()) {
      LOG_ERROR("获取-未找到长连接 {} 对应的客户端信息！", (size_t)conn.get());
      return false;
    }
    uid = it->second.uid;
    ssid = it->second.ssid;
    LOG_DEBUG("获取长连接客户端信息成功！");
    return true;
  }
  void remove(const server_t::connection_ptr &conn) {
    std::unique_lock<std::mutex> lock(_mutex);
    auto it = _conn_clients.find(conn);
    if (it == _conn_clients.end()) {
      LOG_ERROR("删除-未找到长连接 {} 对应的客户端信息！", (size_t)conn.get());
      return;
    }
    auto uid = it->second.uid;
    auto ssid = it->second.ssid;
    auto uit = _uid_sessions.find(uid);
    if (uit != _uid_sessions.end()) {
      uit->second.erase(ssid);
      if (uit->second.empty()) {
        _uid_sessions.erase(uit);
      }
    }
    _ssid_connections.erase(ssid);
    _conn_clients.erase(it);
    LOG_DEBUG("删除长连接信息完毕！");
  }

private:
  std::mutex _mutex;
  // uid -> ssid_list
  std::unordered_map<std::string, std::unordered_set<std::string>>
      _uid_sessions;
  // ssid -> connection
  std::unordered_map<std::string, server_t::connection_ptr> _ssid_connections;
  // connection -> Client
  std::unordered_map<server_t::connection_ptr, Client> _conn_clients;
};

} // namespace MY_IM
