# 校园失物招领 · C++ 后端框架 / MySQL 接入 选型矩阵

> 任务 1 交付物｜调研日 **2026-11-08**｜只读调研（未安装软件、未启停服务、未改系统设置）
> 事实前提：[环境体检](environment-audit.md)（Win11 Insider 26220 / 无 MSVC·CMake·vcpkg / MySQL 8.0 已停用 / 仅 WLAN）
> 核对方式：GitHub Release API + 官方文档 + vcpkg master 端口清单；"最新"= 2026-11-08 当日可验证的最新稳定版。
## 0. 结论速览

| 项 | 结论 |
|---|---|
| 首选框架 | **Drogon v1.9.14**（2026-10-12，MIT）：路由/中间件/WebSocket/multipart/ORM/异步 IO 全带，vcpkg 一键装，仓库 CI 有 Windows-2022 MSVC 构建+测试 |
| 备选 fallback | **cpp-httplib v0.65.0**（2026-11-05，MIT，header-only，自带 WS/multipart/TLS）＋ **libmariadb**（同 DB 层）；触发条件见 §3.4 |
| MySQL 客户端 | **libmariadb（MariaDB Connector/C）3.4.11**（LGPL-2.1，vcpkg `libmariadb`）——Drogon 的 `mysql` 特性在 Windows 上拉的正是它 |
| 否决 | userver（无 Windows）、oatpp（停滞约 1 年）、Pistache（无 WebSocket）、Boost.Beast（无路由/ORM，成本高）、cpprestsdk（已归档）、ODBC（多一层）、websocketpp（2020 后无 release） |
| DB 前置 | MySQL 8.0 已于 **2026-04-30 EOL**；装环境直接上 **9.7 LTS**（至 2034-04）或 8.4 LTS（至 2032-04）。9.7 默认 `caching_sha2_password`，**`mysql_native_password` 已移除** |
| 成本 | 同 MVP 比 Go/Node 多 **5~8 人日**（+45%~70%）；若 C++ 生疏再加 3~5 人日学习成本 → 合计多 6~10 人日，见 §5 |
| 一句话 | C++ 是硬要求 → Drogon + libmariadb + Caddy，M0 用 1 天 spike 验证；只是"听说 C++ 快" → 换 Go/Node 省约 40% 工期 |
## 1. 候选框架对比
### 1.1 版本 / 维护 / Windows / vcpkg / 许可证

| 框架 | 最新稳定版（日期） | 仓库最近提交 | Windows + MSVC | vcpkg 端口 | 许可证 | 维护判断 |
|---|---|---|---|---|---|---|
| Drogon | v1.9.14（2026-10-12） | 2026-11-04 | ✅ CI 有 windows-2022（static/shared + 测试，用 conan） | ✅ 1.9.14 | MIT | 活跃，月级发布 |
| Crow | v1.3.4（2026-09-07）；v1.3.5 预发布 2026-10-03 | 2026-11-07 | ✅ 官方 Windows 文档（vcpkg/MSVC） | ✅ 1.3.5 | BSD-3-Clause | 活跃 |
| oatpp | 1.3.1（2024-12-01） | 2025-11-12 | ✅ 有 vcpkg 端口与 Windows 说明 | ✅ 1.3.1 | Apache-2.0 | **停滞约 1 年，慎选** |
| userver | v3.2（2026-09-09） | 2026-11-06 | ❌ 官方仅 Linux/macOS + GCC/Clang（C++20） | ❌ 无 | Apache-2.0 | 活跃但与本机不兼容 |
| cpp-httplib | v0.65.0（2026-11-05） | 2026-11-07 | ✅ header-only，无构建依赖 | ✅ 0.65.0 | MIT | 极活跃（周级） |
| Boost.Beast | Boost 1.92.0（2026-08-12） | 2026-11-04 | ✅（MSVC 编译 Boost） | ✅ boost-beast 1.92.0 | BSL-1.0 | 活跃（随 Boost） |
| Pistache | v0.4.26（2024-12-23） | 2026-07-11 | ✅ 有 Win 构建脚本（Win11/Server 2022 实测） | ✅ 0.4.26 | Apache-2.0 | 更新慢，无 WS |
| cpprestsdk | v2.10.19（2023-12-05） | **已归档** | ⚠️ 曾支持 | ❌ 端口已移除（2026-11 查询 404） | MIT | **已归档，禁止新项目使用** |
### 1.2 功能矩阵（✅ 内置 / ⚠️ 有限或需外接 / ❌ 无）

| 框架 | WebSocket | multipart 上传 | 路由/中间件 | TLS | ORM/DB | 测试 | 文档 | 上手成本 |
|---|---|---|---|---|---|---|---|---|
| Drogon | ✅ 服务端+客户端 | ✅ HttpFile | ✅ Controller + Filter | ✅ OpenSSL（可选） | ✅ ORM + MySQL/PG/SQLite/Redis | ✅ gtest + CI | 较好（站点+示例多，中文资料多） | 中 |
| Crow | ✅ | ✅ | ✅ 路由 + Middleware | ✅ OpenSSL | ❌ 无 ORM（自写 SQL） | ✅ 自带单测 | 好（crowcpp.org 分章指南） | 低-中（Flask 风格） |
| oatpp | ✅ | ✅ | ✅ Endpoint + Interceptor | ✅ | ⚠️ 有 ORM 与 SQLite/PG 模块，**无官方 MySQL 模块** | ✅ | 好（oatpp.io 教程） | 中-高（DTO/代码生成） |
| userver | ✅ | ✅ | ✅ | ✅ | ✅ 官方 MySQL/PG/Mongo 等驱动 | ✅ 单元+功能 | 很好 | 高（且无 Windows） |
| cpp-httplib | ✅ 阻塞式，每连接一线程 | ✅ | ⚠️ 正则路由 + pre/post handler，无中间件链 | ✅ OpenSSL/mbedTLS/wolfSSL | ❌ | ✅ gtest 测试量大 | 好（单页 README 很全） | 低 |
| Boost.Beast | ✅ | ❌ 需手写解析 | ❌ | ✅ Asio SSL | ❌ | ✅ Boost 测试套件 | 参考级（陡） | 高 |
| Pistache | ❌ | ❌ 未见内置 | ✅ 路由 | ✅ | ❌ | ✅ gtest | 一般（doxygen） | 中 |
| cpprestsdk | 归档 | 归档 | 归档 | 归档 | 归档 | 归档 | 归档 | 不评估 |
### 1.3 否决/保留理由（可执行）

- **userver**：工程化能力最强（MySQL/PG/Mongo 驱动、gRPC、功能测试），但[官方支持平台](https://userver.tech/d0/dd5/md_en_2userver_2supported__platforms.html)只有 Linux/macOS + GCC/Clang，**Windows 不在支持范围**，本机方案直接出局。若将来迁 Linux 服务器，它可作升级首选。
- **oatpp**：最后的 release 是 2024-12 的 1.3.1，仓库 2025-11 后无提交，且 MySQL 无官方模块（官方仅有 SQLite/PostgreSQL 模块）——依赖"半停更 + 缺 MySQL"的框架不划算。
- **Pistache**：无 WebSocket（实时交互要另造轮子），2024-12 后无新 release，文档薄 → 否决。
- **Boost.Beast**：只是 HTTP/WS 协议库，没有路由、没有 multipart、没有 ORM，等于自己写半个框架；至少多 5~10 人日，**只在"学习 Boost.Asio"本身是需求时才考虑**。
- **cpprestsdk**：README 已挂 **ARCHIVED: no longer maintained**，最后 release 2023-12，vcpkg 端口已移除 → 禁止新项目。
## 2. MySQL 接入（Windows + MSVC 视角）
### 2.1 四种方式对比

| 方式 | 2026-11 版本 | 许可证 | Windows/MSVC 实况 | 结论 |
|---|---|---|---|---|
| **libmariadb**（MariaDB Connector/C） | 3.4.11（2026-09-29） | **LGPL-2.1-or-later** | vcpkg 编译，与 VS2022 同源；提供兼容 `mysql.h` 的 C API；**含 caching_sha2 插件**；Windows 构建不带 OpenSSL（本机连 127.0.0.1 不需要） | ✅ **首选** |
| libmysqlclient（Oracle 客户端库） | vcpkg `libmysql` 8.0.46；或已装 MySQL 8.0 自带 lib | GPLv2 + FOSS Exception | 可直连本机已装 8.0 的 lib/dll，但 **8.0 线已 EOL**、GPL 约束、与 9.7 服务端版本错配 | ❌ 不作主选（自用可，分发麻烦） |
| MySQL Connector/C++ | **9.7.0（2026-04-22）**，官方称可用于 MySQL 8.0/8.4/9.7 | GPLv2 + **Universal FOSS Exception v1.0** | 官方 Windows MSI + 需 VC++ Redist；两种 API：**传统 JDBC API**（走 libmysql，无需 X Plugin）与 **X DevAPI**（需 X Plugin/33060 端口 + protobuf）。vcpkg 端口 9.7.0 的 `jdbc` 特性仅支持静态链接，且会编 protobuf/openssl/zstd/lz4，13.8GB 内存的笔记本构建吃紧 | ⚠️ 备选（必须用 Oracle 官方客户端时） |
| ODBC（Connector/ODBC） | 随 MySQL 9.x 发布 | GPLv2 + FOSS Exception | 多一层驱动/DSN，错误码间接、部署多一步、无收益 | ❌ 否决 |

> 第三方封装方向：**Drogon 自带 ORM**（MySQL 后端就是 libmariadb）、sqlpp11（类型安全 SQL，但模板编译重、文档少）。本项目建议 **ORM 可用可不用，SQL 手写在 repository 层 + 参数化 prepared statement**（与 Lead 方案一致）。
### 2.2 libmysqlclient 的 "GPL + FOSS exception" 到底意味着什么

1. **库本身是 GPLv2**：默认情况下链接它的程序在**分发**时要按 GPL 处理（提供源码）。
2. **FOSS Exception 是附加许可**：若你的应用以获批的 FOSS 许可证（GPL/LGPL/BSD/MIT/Apache-2.0 等）分发，可链接客户端库而**不必让整个应用变成 GPL**。注意 Oracle 新版 Connector/C++ 已改用 **Universal FOSS Exception v1.0**（侧重"接口互操作"），经典 MySQL FOSS Exception 的含义仍是"FOSS 应用豁免"。
3. **闭源分发不适用 exception**：把 exe/dll 发给别人（含交给学校部署）就触发 GPLv2 → 要么按 GPL 开源，要么买 Oracle 商业许可。
4. **但"自用服务器"不触发**：GPLv2 的义务来自"分发"，不是"网络服务"（GPL 无网络条款，AGPL 才有）。只在自己笔记本上跑、不对外分发二进制 → 法律风险低；**风险点在"打包发给同学/学校"那一刻**。
5. 实操结论：**优先 LGPL 的 libmariadb**——默认 `x64-windows` 动态 triplet 产出 DLL，随包附 LGPL 声明、允许替换库即可合规；只有静态链接（`x64-windows-static`）才需要额外提供可重链接的目标文件。**别为了 Oracle 官方客户端而承担 GPL 的解释成本**。
### 2.3 Windows+MSVC 上最稳、最省事

- **结论：vcpkg 装 libmariadb（Drogon 的 `mysql` 特性会自动拉）**。与 VS2022 工具链同源编译，无 Oracle 安装包/VC++ Redist 版本对齐问题；调试符号、静态/动态链接都由 vcpkg 统一。Drogon ORM 或直接 C API 都行。
- **认证风险（必须 M0 实测）**：MySQL 9.7 默认 `caching_sha2_password` 且移除了 `mysql_native_password`；libmariadb 3.4 有 `caching_sha2_pw` 插件，但**非 TLS 连接的全量认证需要 RSA 公钥交换**。本机可选：① 连接用 TLS（MySQL 自签证书；但 libmariadb Windows 构建不带 OpenSSL，需自编/triplet，麻烦）；② 明文 TCP + 确认 `caching_sha2_password_auto_generate_rsa_keys`（默认 ON）可用；③ 若不通则改用 Oracle Connector/C++（自带 OpenSSL）或给 MySQL 配自签 TLS。此项已写入 §3.3 spike 验收。
- **别用 MySQL 8.0 安装目录里的 libmysql**：8.0 已 EOL，客户端与 9.7 服务端版本错配，且 GPL。
## 3. 明确推荐
### 3.1 首选组合 + 最小依赖清单

依赖（`backend/vcpkg.json`，manifest 模式，锁 baseline）：

```json
{
  "name": "campus-lostfound-backend",
  "version-string": "0.1.0",
  "builtin-baseline": "（用 vcpkg x-update-baseline 生成）",
  "dependencies": [
    { "name": "drogon", "features": ["mysql", "ctl"] },
    "spdlog",
    "nlohmann-json",
    "gtest"
  ]
}
```

| 用途 | 选择 | 版本 | 理由 |
|---|---|---|---|
| 日志 | **spdlog** | 1.17.0 | header/编译两用，rotating/daily sink 内置；Drogon 可 `USE_SPDLOG=ON` |
| JSON | **jsoncpp（Drogon 自带）为主**；nlohmann/json 3.12.0 仅测试/配置 | — | 控制器/ORM 原生就是 `Json::Value`，避免双 JSON 库来回转换 |
| 测试 | **GoogleTest** | 1.18.0（2026-08-10） | Drogon/vcpkg 生态默认、CI 直接跑；Catch2 3.16.1 可替但编译更慢 |
| 密码哈希 | libsodium（Argon2id） | 1.0.22 | 不自己实现（运维调研已定） |

CMake 要点（MSVC 必踩的坑）：

- 工具链：VS 2022 Build Tools（MSVC 19.4x）+ CMake ≥3.25 + Ninja；设 `VCPKG_ROOT`。
  `cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release`
- MSVC 编译选项：**`/utf-8`**（中文源码/字符串必加）、`/EHsc`、`/W4`、C++20；全局 `NOMINMAX` + `WIN32_LEAN_AND_MEAN`。
- 动态 triplet 对应 `/MD`；**不要混用静态 CRT/静态库**（LNK2038 灾难）。
- 首次构建设 `VCPKG_MAX_CONCURRENCY=2`：本机空闲内存仅 1.66~3.0GB，并行编译易 OOM。
- 部署时把 `drogon.dll / trantor.dll / jsoncpp.dll / mariadb.dll / zlib1.dll / spdlog.dll`（视 triplet）拷到 exe 旁；TLS 交给 Caddy，**Drogon 不编 OpenSSL**（减依赖）。
### 3.2 推荐目录结构

```text
campus-lostfound/
├─ backend/
│  ├─ vcpkg.json  CMakeLists.txt  CMakePresets.json
│  ├─ src/
│  │  ├─ main.cpp                 # 解析 --console，注册信号，app().run()
│  │  ├─ repository/              # 唯一写 SQL 的地方（prepared statement）
│  │  ├─ db/                      # MySqlPool、MySqlStmt（RAII 包 mariadb C API）
│  ├─ tests/                      # gtest：状态机、DAO、路由 单元/集成
├─ web/                           # 前端（另任务）
└─ deploy/                        # winsw/*.xml、Caddyfile、scripts/*.ps1
```
### 3.3 M0 spike 验收（1 天，与 Lead 计划一致）

1. `vcpkg install` 成功（首次含下载 30~60 min；命中二进制缓存 <2 min）。
2. `GET /healthz` → 200 JSON，**中文不乱码**（验证 /utf-8 与 JSON 编码）。
3. 参数化 SQL：建表 + 插入/查询中文数据，prepared statement 防注入。
4. WebSocket echo：浏览器/脚本连上，收发 100 条消息。
5. multipart：上传 5MB jpg，服务端校验 MIME/大小/落盘 + 返回 URL。
6. **MySQL 9.7 caching_sha2 连接实测**（§2.3 风险点）。
7. Ctrl+C → 3 秒内干净退出、连接关闭、日志 flush。

> 任一项卡 >4 小时 → 当天触发 §3.4 fallback 决策（不拖到第 2 天）。
### 3.4 Fallback 触发与切换成本

- **触发条件**：vcpkg 构建 Drogon 失败 >0.5~1 天（trantor/libmariadb/zlib 链接、DLL 缺失、Debug/Release runtime 混用）；或 WebSocket/上传有阻塞性 bug 且 1 天内无解。
- **切到**：`cpp-httplib 0.65.0` + **同一 libmariadb + spdlog + gtest**。路由从 Drogon Controller 改为 lambda 注册（≈0.5 人日）；WS 用 httplib 内置（阻塞式、每连接一线程，50 并发足够；>500 长连接再换 uWebSockets v20/Apache-2.0 或回 Drogon）。
- **DB 层完全不变**——这正是推荐 libmariadb（而非 Drogon ORM 深度绑定）的第二个理由：框架可换，数据访问层不换。
## 4. 本机 Windows 服务化部署要点（C++ 特有）

- **进程拓扑**（与 [lan-security-ops](lan-security-ops.md) 一致）：Caddy 8443 对外 → `127.0.0.1:8080` C++ API；MySQL `127.0.0.1:3306`；对外只开 Caddy 端口。
- **只监听回环**：`app().addListener("127.0.0.1", 8080);`（**不要 0.0.0.0**）；/ws 由 Caddy 反代并自动处理 Upgrade（WSS 在 Caddy 终止）。
- **服务化：WinSW 2.12.0**（2023-01-28，MIT；与运维调研结论一致）。XML 关键项：
  `<executable>%BASE%\lostfound-api.exe</executable>`、`<workingdirectory>%BASE%</workingdirectory>`、`<depend>MySQL80</depend>`、`<stoptimeout>15sec</stoptimeout>`、`<onfailure action="restart" delay="10 sec"/>`、`<log mode="roll-by-time">`（兜底 stdout/stderr）。
  - WinSW 停止时**先向控制台程序发 Ctrl+C，等待至多 15s 再强杀**（[WinSW 文档](https://github.com/winsw/winsw/blob/v3/docs/xml-config-file.md#stoptimeout)）→ Drogon 默认 SIGINT handler 会调 `app().quit()`，形成优雅退出。
  - **NSSM 2.24 不选**：2014-08-31 停更，Win10+ 还需 2.24-101 预发布版（与运维调研一致）。
- **优雅退出清单**：`app().quit()` 后由 `run()` 返回 → 关闭 MySQL 连接池、`mysql_library_end()`、`spdlog::shutdown()`；上传文件先写 `*.part` 再 `MoveFileEx(MOVEFILE_REPLACE_EXISTING)` 原子改名；事务短小 + autocommit；WS 前端自动重连。另提供 `--console` 模式，开发时 Ctrl+C 直接验证同一条退出路径。
- **日志轮转**：spdlog `rotating_file_sink_mt("logs/api.log", 10*1024*1024, 8)` + `flush_on(warn)` + `flush_every(3s)`；合规要求留存 ≥6 个月 → 叠加 `daily_file_sink("logs/api-%Y%m%d.log")` + 计划任务归档与清理；**不要在 Windows 上外挂 logrotate**。日志目录加杀软/索引排除，避免文件被占用导致重命名失败。WinSW 日志只兜底崩溃栈，不与 spdlog 重复写业务日志。
- **启动顺序与自愈**：MySQL80（Automatic）→ lostfound-api（Delayed Start + `<depend>`）→ caddy；计划任务每 5 分钟 `GET https://127.0.0.1:8443/healthz`（内含 DB ping），失败重启服务并告警。
## 5. 成本量化（估算：1 人、有 C++ 语法基础、不熟 Web 全栈；口径=能演示可试用的 MVP）
### 5.1 人日对比

| 环节 | C++ / Drogon | Go（Gin） | Node（Fastify） |
|---|---|---|---|
| 环境 + 工具链 | 1.0–2.0 | 0.5 | 0.5 |
| DB/DAO + 迁移 | 3.0–4.0 | 2.0–2.5 | 2.0–2.5 |
| 鉴权/中间件/参数校验 | 3.0–4.0 | 1.5–2.0 | 1.5–2.0 |
| 业务状态机/事务 | 3.0–3.5 | 2.5–3.0 | 2.5–3.0 |
| WebSocket 实时 | 1.0–1.5 | 1.0 | 1.0 |
| 上传/静态/文件 | 1.0–1.5 | 0.5–1.0 | 0.5–1.0 |
| 测试 | 2.0–2.5 | 1.5 | 1.5 |
| 服务化/部署 | 1.0 | 0.5 | 0.5 |
| **合计** | **15.5–20** | **10–12** | **10–11.5** |

> 结论：**多 5~8 人日（+45%~70%）**；若 C++ 生疏，学习曲线再 +3~5 人日 → 实际多 6~10 人日。与 Lead 文档"多 1.5~2.5 倍""可换语言省 30~40% 工期"的区间一致（取上界还是下界取决于 C++/vcpkg 熟练度）。
### 5.2 学习曲线集中在 4 处

1. **工具链**：MSVC + vcpkg + CMake + DLL/CRT 一致性问题，首次配置 1~2 天，是最大的"一次性税"。
2. **内存/生命周期/并发**：异步回调捕获悬垂、连接池线程安全、无 GC 兜底；崩溃通常没有友好堆栈，需 dump/日志定位。
3. **生态缺口**：没有开箱的 JWT/校验/上传处理/邮件/队列；要自己拼 OpenSSL、libsodium、spdlog。小项目里这些"拼装"是隐性人日。
4. **迭代速度**：改一行要重编译链接、无热重载；靠 OpenAPI 契约 + 前端 Mock 并行对冲（Lead 方案已采用）。
### 5.3 什么情况下应劝用户换后端

- **C++ 是课程/毕设硬要求** → 继续 C++，用 Drogon 把轮子成本降到最低（本报告结论适用）。
- 否则出现任一条即建议换 **Go（Gin/Echo）或 Node（Fastify）**：① 交付期 <4 周且只有 1 人；② 团队无 C++ 项目经验；③ 需求会频繁变、需要快速迭代；④ 需要成熟生态（OAuth、Web Push、图片处理、后台任务）；⑤ 目标只是"给同学用的失物招领"，性能不是瓶颈（本项目瓶颈在网络/图片/DB，C++ 性能优势用不上）。
- 折中：后端语言不变，但先定 OpenAPI 契约、前端独立开发；管理脚本/数据迁移用 Python/Node。
## 6. 风险与未决

| 风险 | 影响 | 对策 |
|---|---|---|
| vcpkg 首次构建 OOM（空闲内存 1.66~3.0GB） | 构建失败/机器卡死 | `VCPKG_MAX_CONCURRENCY=2`、分步安装、关浏览器；失败即 fallback |
| Drogon Windows 使用面小于 Linux | 冷门问题难搜 | 锁 1.9.14；1 天 spike；保留 cpp-httplib 退路 |
| MySQL 9.7 caching_sha2 + 非 TLS TCP | 连不上数据库 | M0 实测；备选 MySQL 自签 TLS / Connector/C++ |
| LGPL/GPL 边界 | 分发合规 | 用动态 triplet；不随包分发 Oracle 客户端；保留 LGPL 声明 |
| WinSW 停更（2.12.0 2023；v3 仍 alpha） | 服务壳风险 | 2.12.0 稳定可用；sc.exe + 计划任务健康检查兜底 |
| **未决**：MySQL 8.0 → 9.7 升级路径 | 装环境时卡壳 | 数据量小 → 逻辑备份 dump → 全新安装 9.7 → import，绕开跨大版本升级链；需 DB 负责人确认 |
## 7. 来源（2026-11-08 核对）

- Drogon：[releases](https://github.com/drogonframework/drogon/releases)（v1.9.14 / 2026-10-12）、[Windows CI](https://github.com/drogonframework/drogon/blob/master/.github/workflows/cmake.yml)、[vcpkg 端口](https://github.com/microsoft/vcpkg/blob/master/ports/drogon/vcpkg.json)（features: ctl/mysql/orm/postgres/redis/sqlite3/yaml）
- Crow：[releases](https://github.com/CrowCpp/Crow/releases)、[Windows 安装文档](https://crowcpp.org/master/getting_started/setup/windows/)、[vcpkg 端口](https://github.com/microsoft/vcpkg/blob/master/ports/crow/vcpkg.json)
- oatpp：[releases](https://github.com/oatpp/oatpp/releases)、[oatpp-postgresql](https://github.com/oatpp/oatpp-postgresql)（无官方 MySQL 模块）
- userver：[releases](https://github.com/userver-framework/userver/releases)、[Supported Platforms](https://userver.tech/d0/dd5/md_en_2userver_2supported__platforms.html)
- cpp-httplib：[releases](https://github.com/yhirose/cpp-httplib/releases)（v0.65.0）、[WebSocket 说明](https://github.com/yhirose/cpp-httplib/blob/master/README-websocket.md)
- Boost 1.92.0：[releases](https://github.com/boostorg/boost/releases)、[boost-beast vcpkg 端口](https://github.com/microsoft/vcpkg/blob/master/ports/boost-beast/vcpkg.json)
- MySQL 生命周期：[endoflife.date/mysql](https://endoflife.date/mysql)（8.0 EOL 2026-04-30；9.7 LTS 2026-04-21 起至 2034-04）
- MySQL 9.7 认证：[Caching SHA-2 Pluggable Authentication](https://docs.oracle.com/cd/E17952_01/mysql-9.7-en/caching-sha2-pluggable-authentication.html)（默认 caching_sha2；mysql_native_password 已移除）
- Connector/C++ 9.7：[9.7.0 release notes](https://docs.oracle.com/cd/E17952_01/connector-cpp-relnotes-en/news-9-7-0.html)（2026-04-22）、[API 说明](https://docs.oracle.com/cd/E17952_01/connector-cpp-9.7-en/connector-cpp-introduction.html)、[Windows 安装](https://docs.oracle.com/cd/E17952_01/connector-cpp-9.7-en/connector-cpp-installation-binary.html)
- 许可证：[Universal FOSS Exception v1.0](https://oss.oracle.com/licenses/universal-foss-exception/)；Connector/C++ LICENSE.txt（GPLv2 + Universal FOSS Exception）；MariaDB Connector/C LGPL-2.1
- MariaDB Connector/C：[v3.4.11 / 2026-09-29](https://github.com/mariadb-corporation/mariadb-connector-c/releases)、[caching_sha2 插件源码](https://github.com/mariadb-corporation/mariadb-connector-c/tree/master/plugins/auth)、[vcpkg libmariadb 3.4.11](https://github.com/microsoft/vcpkg/blob/master/ports/libmariadb/vcpkg.json)
- 工具链版本：spdlog 1.17.0、nlohmann/json 3.12.0（vcpkg 端口）；[Catch2 v3.16.1](https://github.com/catchorg/Catch2/releases)、[GoogleTest v1.18.0](https://github.com/google/googletest/releases)
- 服务壳：[WinSW v2.12.0](https://github.com/winsw/winsw/releases)、[WinSW 停止/超时文档](https://github.com/winsw/winsw/blob/v3/docs/xml-config-file.md#stoptimeout)、[NSSM](https://nssm.cc/download)（2.24，Win10+ 需 2.24-101）
- 成本参考：[Go at Google](https://go.dev/talks/2012/splash.article)（编译/依赖复杂度与开发效率的权衡依据；人日为本报告估算，非实测）