# chat-server-cpp

基于 C++ 实现的即时通讯（IM）服务端，采用「网关 + 多子服务」的微服务架构。

网关是客户端的唯一入口，对外提供 **HTTP**（请求-响应类业务）与 **WebSocket**（服务端主动推送通知）两条通道；内部各子服务之间通过 **brpc + Protobuf** 做 RPC 调用，由 **etcd** 完成服务注册与发现；MySQL / Redis / Elasticsearch / RabbitMQ 分别承担业务数据存储、登录会话管理、历史消息全文检索与消息异步落库。

仓库同时包含配套客户端：`client/` 是基于 Qt6 的桌面客户端（HTTP + WebSocket 双通道、Protobuf 协议），`client_cpp/` 是按子服务拆分的一组 HTTP 接口测试客户端。

规模：8 个可独立部署的服务进程、10 个 `.proto` 契约、37 个 RPC 接口，服务端约 9.4k 行 C++（不含 protobuf / ODB 生成代码）。

## 功能

| 模块 | 能力 |
| --- | --- |
| 用户 | 用户名注册/登录、手机号验证码注册/登录、个人信息（头像 / 昵称 / 签名 / 手机号）读写 |
| 好友 | 用户搜索、好友申请与同意/拒绝、离线待处理申请拉取、删除好友、好友列表 |
| 会话 | 单聊会话（对方同意好友时创建）、群聊会话手动创建、群成员添加与查看、会话列表 |
| 消息 | 文本 / 图片 / 文件 / 语音消息收发；最近 N 条、按时间段查询历史、关键字全文检索 |
| 频道 | 频道创建、频道成员添加、频道消息广播 |
| 多端在线 | 同账号多设备同时在线，一条通知推送到该账号名下所有在线连接，互不顶号 |
| 离线补齐 | 离线通知落库 + 客户端 ACK；上线后按设备维护的 `cur_max_message_id` 增量拉取缺失消息 |
| 其他 | 语音消息转文字（云端 ASR）、头像与消息附件的单个/批量上传下载 |

鉴权模型：登录成功后服务端创建登录会话并下发 `session_id`，除注册 / 登录 / 验证码获取外，所有请求都必须携带 `session_id`，WebSocket 建连后的首包也用它完成身份绑定。

## 架构与子服务

| 子服务 | 目录 | 默认端口 | 职责 |
| --- | --- | --- | --- |
| 网关 gateway | `server/gateway` | HTTP `9000` / WS `9001` | 客户端唯一入口：会话鉴权、请求分发到子服务、长连接管理与通知推送 |
| 用户 user | `server/user` | `9190` | 注册登录、验证码、用户信息读写、用户信息入 ES 供搜索 |
| 语音 speech | `server/speech` | `9191` | 调用云端 ASR SDK 完成语音转文字 |
| 文件 file | `server/file` | `9292` | 头像 / 附件 / 语音文件的存取（单个与批量） |
| 频道 channel | `server/channel` | `10002` | 频道创建、成员添加、频道消息广播 |
| 转发 transmite | `server/transmite` | `10004` | 组装完整消息、计算转发目标列表、把消息投递进 MQ |
| 消息 message | `server/message` | `10005` | 消息落库与检索、离线通知存储与投递、增量同步 |
| 好友 friend | `server/friend` | `10006` | 好友关系、好友申请、聊天会话与会话成员管理 |

HTTP 接口统一以 `/chat/<模块>/<动作>` 命名（如 `/chat/user/username_login`、`/chat/message_transmit/new_message`），请求与响应正文均为 Protobuf 序列化数据，`POST` 方法。

### 消息主链路

```
客户端 --HTTP /chat/message_transmit/new_message--> 网关
        --RPC--> 转发子服务（组装消息 / 查会话成员得到转发目标 / Publish 到 RabbitMQ）
        --返回目标列表--> 网关 --WebSocket--> 目标用户名下的每一条在线连接
                                              RabbitMQ --Consume--> 消息子服务
                                                文本 -> Elasticsearch
                                                图片/文件/语音 -> 文件子服务换回 file_id
                                                消息元信息 -> MySQL
```

推送侧的连接管理由网关的 `Connection` 维护三组映射：`session_id -> connection`（精确定位单条连接，断线 / 踢线只动当前会话）、`uid -> {session_id}`（按用户聚合，推送时遍历全部在线连接）、`connection -> (uid, session_id)`（断线时反查清理）。设计与改造过程见 [docs/多设备登录与跨端消息同步.md](docs/多设备登录与跨端消息同步.md)。

## 技术栈

| 类别 | 选型 |
| --- | --- |
| RPC / 序列化 | brpc、Protobuf |
| 服务治理 | etcd（`etcd-cpp-apiv3`）做服务注册与发现，子服务实例上下线由 `ServiceManager` 动态维护信道 |
| 网关通信 | cpp-httplib（HTTP）、websocketpp（WebSocket 长连接） |
| 存储 | MySQL（ODB 做 ORM）、Redis（`redis++`/hiredis，登录会话与在线状态、验证码、序列号）、Elasticsearch（`elasticlient` + `cpr`，用户与消息检索） |
| 消息队列 | RabbitMQ（AMQP-CPP + libev） |
| 基础设施 | gflags（运行参数）、spdlog（日志）、gtest（单元测试）、CMake（构建） |
| 云能力 | 百度语音识别（`aip-cpp-sdk`）做语音转文字，阿里云短信做验证码下发 |

## 目录结构

```
server/
├── gateway/ user/ friend/ message/ transmite/ file/ speech/ channel/
│            每个子服务为 source/（实现）+ test/（测试客户端）+ CMakeLists.txt
├── common/  公共封装：etcd.hpp、channel.hpp、mysql*.hpp、redis.hpp、
│            essearch.hpp、rabbitmq.hpp、asr.hpp、logger.hpp、utils.hpp
├── proto/   10 个契约文件：base / user / friend / message / transmite /
│            file / speech / channel / gateway / notify
├── odb/     ODB 持久化类定义
└── sql/     建表脚本：user、relation、friend_apply、chat_session、
             chat_session_member、message、notify、channel
client/      Qt6 桌面客户端（model / network / proto / resource）
client_cpp/  按子服务拆分的 HTTP 接口测试客户端
docs/        设计与改造记录
test/        软件测试报告
```

## 构建与运行

依赖的中间件默认地址：etcd `127.0.0.1:2379`、MySQL（库名 `IM`）、Redis `127.0.0.1:6379`、Elasticsearch `127.0.0.1:9200`、RabbitMQ `127.0.0.1:5672`。先用 `server/sql/*.sql` 建表。

构建（out-of-source，产物在 `server/build/bin`）：

```bash
cd server && mkdir -p build && cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && make -j$(nproc) && make install
```

启动顺序：中间件 → 各业务子服务（自行向 etcd 注册）→ 网关（从 etcd 发现子服务）。所有地址、端口、账号密码等都是 gflags 参数，可按需覆盖：

```bash
./bin/user_server --etcd_host=http://127.0.0.1:2379 --listen_port=9190 --mysql_pswd=<your-password>
```

查看 etcd 中已注册的实例：

```bash
ETCDCTL_API=3 etcdctl --endpoints=http://127.0.0.1:2379 get /service --prefix
```
