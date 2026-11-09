# 校园失物招领 · 局域网 / HTTPS / 运维 / 安全 / 合规 调研

> 任务 3 交付物｜调研日 2026-11-08｜仅只读调研（未安装软件、未改系统配置）｜"本机补测"= 当日只读命令实测｜事实前提：`research/environment-audit.md`（Win11 家庭版 Insider 26220 / Ryzen 5 5500U / 13.85GB / 仅 WLAN 校园 Wi-Fi 192.168.1.100/24 / 全配置防火墙且当前 Public / MySQL 8.0 已装停用 / 无 C++·Node·Caddy）
## 0. 摘要与版本基线（2026-11-08 核对）
1. **先验证网络再选型**：校园 Wi-Fi 客户端隔离决定"本机做服务器"是否成立，用 §1.3 五分钟实测判定；不通过则切 §1.5 方案 B（自带路由器独立局域网）或用 Tailscale 兜底。
2. **HTTPS 主选**：真域名（公网 DNS 的 A 记录直接指向内网 IP）+ Let's Encrypt **DNS-01**（无需公网入站、手机零配置）；**零成本备选**：Caddy 内部 CA（`tls internal`）+ 各设备装根证书。反代选 **Caddy v2.11.9 + WinSW 2.12.0**。
3. **7x24 头号风险是软件不是硬件**：Insider 自动重启、睡眠/合盖、内存实测只剩 1.66GB；靠 §4 电源策略 + 服务自启 + 资源预算解决；**远程维护注意家庭版不能做 RDP 服务端**。
4. **合规红线**：不采集人脸/身份证；手机号学号脱敏；日志留存 ≥6 个月（网安法第 21 条）；真实数据不出境（Cloudflare Tunnel 只做演示）；向学校信息化/网安部门报备。
5. **MySQL 8.0 已于 2026-04-30 EOL**（[endoflife](https://endoflife.date/mysql)），装环境时就该换 **8.4 LTS**（至 2032-04）或 9.7 LTS，不要沿用体检报告里的 8.0。
### 0.1 版本核对（官方源）

| 组件 | 2026-11-08 状态 | 结论 |
|---|---|---|
| Caddy | **v2.11.9**（2026-11-05，[releases](https://github.com/caddyserver/caddy/releases)） | 选它；生产可先锁 v2.11.8 观察 |
| mkcert | **v1.4.4**（2022-04-26 后停更，[releases](https://github.com/FiloSottile/mkcert/releases)） | 可用但不必需，内部 CA 交给 Caddy |
| WinSW / NSSM | WinSW **v2.12.0**（2023-01-28，[releases](https://github.com/winsw/winsw/releases)）｜NSSM **2.24**（2014-08-31，[nssm.cc](https://nssm.cc/download)，Win10+ 需 2.24-101 预发布） | 选 WinSW；NSSM 停更 12 年，否决 |
| cloudflared | **2026.10.7**（2026-10-30，[releases](https://github.com/cloudflare/cloudflared/releases)） | 仅演示备选 |
| Tailscale | **v1.104.5**（2026-11-03，[releases](https://github.com/tailscale/tailscale/releases)）；Personal 免费版 6 用户、设备不限（[pricing](https://tailscale.com/pricing)） | 运维通道 + 跨网兜底 |
| MySQL / Node.js | MySQL 8.0 EOL **2026-04-30**，8.4 LTS 至 2032-04、9.7 LTS 至 2034-04（[endoflife](https://endoflife.date/mysql)）；Node **26.13.0** Active LTS、24.22.2 维护 LTS（[endoflife](https://endoflife.date/nodejs)） | DB 升 8.4 LTS；前端构建用 Node 24/26 LTS |
| libsodium | **1.0.22**（2026-04-10，[releases](https://github.com/jedisct1/libsodium/releases)）；argon2 参考实现停在 20190702 | C++ 用 libsodium 做 Argon2id |
| 中国法规 | PIPL 2021-11-01；《网络数据安全管理条例》2025-01-01；《人脸识别技术应用安全管理办法》2025-06-01；网络安全法修正 **2026-01-01** 施行（[主席令](http://www.npc.gov.cn/npc/c1773/c1848/c21114/wlaqfxz/wlaqfxz002/202511/t20251103_449246.html)） | 见 §7 |
| 本机补测 | 空闲内存 **1.66GB**（比体检更紧）；ACME 出站 443 可达（`acme-v02.api.letsencrypt.org:443=True`）；存在 VMware 三张虚拟网卡；S3 可用、休眠/快速启动关闭；AC 下当前 3 小时自动睡眠 | DNS-01 网络前提成立；必须先清启动项 |
## 1. 局域网可达性（前置否决项）
### 1.1 同 SSID 手机访问本机：三件事缺一不可
1. **二层可达**：同 AP/同子网且未开客户端隔离（AP Isolation / Client Isolation / 端口隔离）。
2. **三层可达**：手机与本机同网段或校园网允许互访（跨 VLAN 默认被 ACL 拦）。
3. **本机放行**：Windows 防火墙对 **Public** 配置放行入站（当前 Public 默认阻止），且服务不能只绑 127.0.0.1。附加风险：Portal 会周期重认证，手机"私有 Wi-Fi 地址"（MAC 随机化）导致重新分配 IP/重认证，加剧 IP 漂移。
### 1.2 校园网/公共 Wi-Fi 五类风险

| 风险 | 表现 | 影响 |
|---|---|---|
| 客户端隔离（AP Isolation） | 手机能上网但访问同网段主机超时 | **方案不成立**（校园网多默认开启，最常见） |
| 跨 VLAN / 大二层 ACL | 10.40/16 内不同楼宇段互访被拦 | 仅同楼/同 AP 可用 |
| Portal 认证超时 | 一段时间后掉线需重认证 | 服务"间歇性失联" |
| 80/443 入站策略拦截 | 高位端口可通、标准端口不通 | 改 8443/9443（§3.4） |
| 准入/NAC（DHCP Snooping、IP Source Guard） | 手工静态 IP 被断网 | **禁止**在校园网段自配静态 IP；无线本身还会 2.4G 干扰/漫游/节能断连，WSS 长连接必须自动重连 |
### 1.3 五分钟实测（本机已装 Python，可直接跑）
```powershell
# ① D:\tmp 放 hello.txt，起临时 HTTP 服务
cd D:\tmp; python -m http.server 8000 --bind 0.0.0.0
# ② 另开【管理员】PowerShell 放行 Public 入站（只加一条，测完删）
New-NetFirewallRule -DisplayName "lan-test-8000" -Direction Inbound -Protocol TCP -LocalPort 8000 -Action Allow -Profile Public
# ③ 手机连同一 Wi-Fi 访问 http://192.168.1.100:8000/
# ④ 成功时 ① 窗口会打印手机 IP；无任何日志=被上游拦截
Remove-NetFirewallRule -DisplayName "lan-test-8000"   # ⑤ 清理
```

| 现象 | 判定 | 下一步 |
|---|---|---|
| 能打开且日志出现手机 IP | 无隔离，方案可行 | 继续 §2，先解决 IP 漂移 |
| 超时/拒绝，换 8443、80、443 均不通 | 客户端隔离或跨 VLAN | 走 §1.5 方案 B/E |
| 8443 通、80/443 不通 | 仅端口策略拦截 | HTTPS 用 8443；证书走 DNS-01（不依赖入站） |
| 手机 IP 不在 10.40/16 | 不同网段/NAT | 换同段设备复测或方案 B |
| 能通但频繁掉线 | Portal 超时/MAC 随机化 | 关"私有 Wi-Fi 地址"，关网卡节能（§4.1） |
> 手机禁 ICMP 是常态，**不要用 ping 判死**；以 HTTP 日志/服务端 `Get-NetTCPConnection` 为准。
### 1.4 无线 vs 有线、IP 漂移、mDNS 与 hosts
- **无线做服务器**可用但不稳（漫游/干扰/节能）；有条件加 **USB 千兆网卡**接有线（宿舍墙口或自带路由器 LAN），延迟与丢包明显改善。
- **DHCP 变化**：校园网租约可能仅数小时，IP 一变书签/二维码全失效。手段：① 找网络中心申请固定 IP（通常需报备审批）；② **自带路由器 LAN 内 DHCP 保留/静态绑定（推荐）**；③ 严禁在校园网段手工配静态 IP（冲突 + 准入断网）。
- **mDNS（`.local`）现实**：iOS/macOS 支持；**Android 系统解析器不支持 `.local`**（[Google issue #140786115](https://issuetracker.google.com/issues/140786115) 仍是功能请求），Chrome 打不开；Windows 内置 mDNS 但**多标签 `.local` 已知解析失败**（[实例](https://github.com/halos-org/halos-mdns-publisher/issues/37)）→ 手机端不能依赖 mDNS；**hosts** 只能改 Windows（`C:\Windows\System32\drivers\etc\hosts`），iOS 不支持、Android 需 root → 仅适合固定演示笔记本。
### 1.5 三套拓扑（按推荐度）

| 方案 | 做法 | 优点 | 缺点 | 适用 |
|---|---|---|---|---|
| **B 自带路由器（推荐）** | 旅行/家用路由器建私有网段（如 192.168.8.0/24），本机有线接 LAN 且 IP 固定；手机连该路由 Wi-Fi | 完全可控、无 AP 隔离、IP 固定、易加 UPS | 仅覆盖该 Wi-Fi；上行接校园网仍需 Portal | 答辩/试运行/宿舍小范围 |
| A 校园 Wi-Fi 直连 | 本机+手机同 SSID | 零成本、覆盖广 | 依赖隔离策略、IP 漂移、Public 防火墙 | 仅当 §1.3 实测通过 |
| E 隧道/热点 | Tailscale 装本机+每台手机；或手机开热点 | 绕过隔离与跨网、自带可信 HTTPS；热点同 NAT 互通 | 每台设备装客户端；热点限设备数/耗电 | 跨网兜底、1–2 人演示 |
## 2. HTTPS 方案对比（本方案必答题）
### 2.1 为什么必须 HTTPS
- **secure context 硬门槛**：Service Worker/PWA 安装、通知、`getUserMedia` 摄像头流、剪贴板、定位，在非 localhost 的 HTTP 下全部不可用；`https://` 页面里的 `ws://` 属混合内容会被拦 → **实时交互必须 wss://**。
- 内网 IP 属 RFC1918 私有地址，**任何公共 CA 都不会为内网地址签证书** → 要么自建信任链，要么"域名解析到内网 IP"。
### 2.2 方案对比

| 方案 | 手机受信？ | 前置条件 | 工作量 | 主要风险 | 结论 |
|---|---|---|---|---|---|
| 自签 / mkcert | ❌ 自签每次警告；mkcert 需装 rootCA | 每台设备装根证书 | 0.5–1h/人 | root key 泄露=可被 MITM；iOS/Android 安装坑多 | 自签否决；mkcert 仅备选 |
| **Caddy 内部 CA** | 需装 rootCA（`caddy trust` 一键装 Windows） | Caddy 提供 root.crt 下载 | 本机 10min + 每设备 10min | 用户不装即报错；换机后要重装 | **零成本首选** |
| **真域名 + DNS-01** | ✅ 公共 CA，零设备配置 | 域名（¥30–100/年）+ 可编程 DNS | 2–4h | DNS API token 权限；A 记录指私网 IP 需 DNS 商允许 | **正式首选** |
| 隧道（Cloudflare / Tailscale） | ✅ | CF 账号+域名+cloudflared；或每台设备装 Tailscale | 1–2h | 流量经境外→**PIPL 出境风险**；断外网不可用；UDP 打洞失败降级中继 | CF 仅公网演示；Tailscale 作运维/跨网兜底 |
### 2.3 推荐顺序与落地要点
1. **正式首选：真域名 + 公网 DNS A 记录指向内网 IP + Let's Encrypt DNS-01**。DNS-01 只校验 DNS TXT，**无需 80/443 入站**（本机实测到 ACME 的 443 出站可达）。关键技巧：A 记录直接写私有 IP（如 `lost.example.com → 192.168.1.100`），手机用公共 DNS 也能解析到内网地址，**无需自建内网 DNS**。实现二选一：① [Caddy 下载页](https://caddyserver.com/download) 勾选 DNS 提供商插件生成 Windows 单文件（免装 Go）；② [win-acme](https://github.com/win-acme/win-acme)（v2.2.9.1，2024-05 后停更但可用）跑 DNS-01 出 PEM，Caddy 用 `tls cert.pem key.pem` 加载。
2. **零成本/离线备选：Caddy `tls internal`**。本机 `caddy trust` 自动装入 Windows 信任库；其他设备装 Caddy 数据目录下 `pki/authorities/local/root.crt`。**证书 SAN 必须含实际访问的 IP/域名**（Caddy 按 site address 自动生成 IP SAN）；根证书私钥只在本机，勿外发。
3. **运维/跨网：Tailscale**。Caddy 对 `.ts.net` 有专门支持（向本机 tailscaled 取证书），配 `tailscale serve` 甚至不写 TLS 配置；但每台手机要装客户端，不适合全体学生。
4. **Cloudflare Tunnel 只作公网演示**：官方文档明确 cloudflared 只建**出站**连接、无需公网 IP（[docs](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/)）。真实学号/手机号/照片经境外节点有出境合规问题（§7）→ 不承载真实数据；**自签证书直接否决**（Chrome 仍禁用 SW/摄像头，且训练用户"忽略警告"）。
### 2.4 根证书安装步骤与坑（内部 CA 路线必读）
- **iOS/iPadOS**：Safari 打开 root.crt（邮件/AirDrop/HTTP 下载）→ 设置顶部"已下载描述文件"→ 安装 → **设置→通用→关于本机→证书信任设置→对该根证书开启"完全信任"**（漏这步=不生效）；必须用 Safari 下载；部分版本需设备密码。
- **Android 7+**：设置→安全→加密与凭据→安装证书→CA 证书（各厂商路径不同）。坑：**用户安装的 CA 默认不被 App/WebView 信任**（需 app 配 network_security_config），Chrome 浏览器上网可以；装完重启浏览器。
- **Windows**：双击 root.crt 安装到"受信任的根证书颁发机构/本地计算机"，或 `certutil -addstore -root root.crt`；Chrome/Edge 跟随系统库。
- **Firefox 独立信任库**：默认不读系统 CA，需设 `security.enterprise_roots.enabled=true` 或手动导入 NSS；其他坑：证书过期/换机要全体重装；**内部 CA + 长 HSTS 会"锁死"访问**（慎用 max-age）；杀软/代理可能替换证书。
## 3. 前置反向代理
### 3.1 选型对比

| 方案 | Windows 体验 | 自动 HTTPS/内部 CA | 反代+WSS | 静态站点 | 结论 |
|---|---|---|---|---|---|
| **Caddy v2.11.9** | 单 exe 无依赖，配置 10 行起 | ✅ 全自动（ACME + 内部 CA + `caddy trust`） | ✅ 自动处理 Upgrade；**重载会断 WS** | ✅ | **选它** |
| nginx / IIS | nginx Windows 版官方定位实验性、单进程；IIS 家庭版无组策略且站点/证书/ARR/WS 四处配置 | ❌ 需手工 certbot 或四处点选 | nginx 需手写 Upgrade 头，IIS 需装 ARR+WS | ✅ | 否决 |
| C++ 自建 TLS | 需 OpenSSL 集成 | ❌ 无 ACME/OCSP/会话复用 | 要自己写 HTTP/1.1+WS 栈 | 弱 | 否决（时间黑洞） |
### 3.2 最小可用 Caddyfile（内部 CA + 反代 + 静态 + 安全头）
```caddyfile
{
  storage file_system D:\campus-lostfound\ops\caddy-data
  admin 127.0.0.1:2019
}
https://192.168.1.100:8443 {        # 有域名时改为 lost.example.com:8443 + tls { dns ... }
  tls internal
  header {
    X-Content-Type-Options nosniff
    Referrer-Policy no-referrer
    Content-Security-Policy "default-src 'self'; img-src 'self' data:; frame-ancestors 'none'"
    -Server
  }
  handle /api/* { reverse_proxy 127.0.0.1:8080 }
  handle /ws*   { reverse_proxy 127.0.0.1:8080 }   # WSS 由 Caddy 自动升级
  handle { root * D:\campus-lostfound\web; try_files {path} /index.html; file_server }
}
```
> **运维要点**：Caddy 默认在配置重载时**强制关闭所有 WebSocket 连接**（[官方文档](https://caddyserver.com/docs/caddyfile/directives/reverse_proxy)），前端必须做断线重连+指数退避。C++ 服务与 MySQL 只绑 `127.0.0.1`，对外只暴露 Caddy 的 8443。
### 3.3 注册为 Windows 服务
```powershell
sc.exe create caddy start= auto binPath= "C:\caddy\caddy.exe run --config C:\caddy\Caddyfile --adapter caddyfile"
sc.exe failure caddy reset= 86400 actions= restart/60000/restart/60000/restart/60000
# 推荐改用 WinSW 2.12.0（Caddy 官方文档推荐）：lostfound-caddy.xml 配 <onfailure action="restart" delay="10 sec"/>
```
> 改配置用 `caddy reload --config Caddyfile`（Windows 服务不能 reload 只能 restart）；Caddy 数据目录务必用 `storage` 指到 D 盘固定路径，避免升级/换用户丢 CA 与证书缓存。
### 3.4 80/443 是否被校园网拦截：实测方法
- 不能假设可用：先在 §1.3 用 80/443/8443/9443 各测一次，记录手机能连哪些端口。若 80/443 被封：**证书走 DNS-01 不受影响**，对外直接用 8443，二维码/书签写 `https://...:8443`。
- 若高位端口也封：说明是客户端隔离而非端口策略，回 §1.5；HTTPS 走 Tailscale（ts.net）。
## 4. 7x24 运行
### 4.1 电源/睡眠/合盖/电池
```powershell
# 交流电下：不睡眠、不休眠、合盖不动作（0=不采取任何操作）
powercfg /change standby-timeout-ac 0
powercfg /change hibernate-timeout-ac 0
powercfg /setacvalueindex SCHEME_CURRENT 4f971e89-eebd-4455-a8de-9e59040e7347 5ca83367-6e45-459f-a27b-476b1d01c936 0
powercfg /setactive SCHEME_CURRENT
powercfg /q SCHEME_CURRENT SUB_BUTTONS LIDACTION    # 复核
Disable-NetAdapterPowerManagement -Name WLAN         # 关无线网卡节能（管理员）
```
- **电池当 UPS**：断电可撑 1–3 小时，但要设"低电量 20% 休眠/关机"防硬断电；长期插电 100% 伤电池，用 Lenovo Vantage 保养模式（约 60%）。若要求 RPO≤15min 且宿舍断电频发，加 500VA UPS（约 ¥300–500）。显示器可 10 分钟关闭（不影响服务），休眠/快速启动保持关闭。
### 4.2 服务化 + 崩溃自启 + 开机自启
- 三个常驻进程都用 **WinSW 2.12.0** 包服务：`caddy`、`lostfound-api`（C++）、`MySQL80` 官方服务改 Automatic；每个配 `<onfailure action="restart" delay="10 sec"/>` + 启动类型 Automatic (Delayed Start)；**MySQL 先起、API 后起**（WinSW `<depend>` 或脚本重试）。
- 兜底：计划任务"开机时" + 每 5 分钟健康检查（`Invoke-WebRequest https://127.0.0.1:8443/healthz` 失败则重启服务并推送告警到企业微信/钉钉机器人）。NSSM 不选（停更 12 年）。
### 4.3 Windows Update 自动重启（最大黑天鹅）
- 家庭版无组策略：用**活动时间** + **暂停更新（最多 5 周）** + 计量连接限制后台下载；每周日固定维护窗口主动更新并重启。
- **Insider Preview（本机 26220，Dev/Beta 通道）会频繁推 build 并可能强制重启**（[官方博客](https://blogs.windows.com/windows-insider/)）：正式运行前必须"停止获取预览版"；若本机 build 高于正式版，只能等正式版追平或重装 → **试运行机不要留在 Insider**；兜底=所有服务自启 + 开机自检 + 恢复后自动推送通知，即使被更新重启也能 5 分钟内恢复。
### 4.4 远程维护（Home 版限制）
- **Windows 11 家庭版不能做 RDP 服务端**：微软文档明确"被连接的电脑必须运行 Pro 版"（[MS Support](https://support.microsoft.com/en-us/windows/how-to-use-remote-desktop-5fe128d5-8fb1-7a23-3b8a-41e636865e8c)）→ 用 **OpenSSH Server 可选功能 + Tailscale/ZeroTier** 组网后 SSH；图形化用 RustDesk（可自建中继）/ToDesk；或升级 Pro 用 RDP（绑 Tailscale 网段，不暴露公网）。最小权限：单独运维账号 + 公钥登录 + 禁密码登录。
### 4.5 资源预算（13.85GB 总量、实测空闲仅 1.66GB）

| 常驻项 | 目标占用 | 措施 |
|---|---|---|
| Windows 基线（Defender/VMware/输入法） | 5.5–7 GB | 关 VMware 自启、OneDrive、Xbox/Game Bar、Edge 后台 |
| MySQL 8.4 | 0.5–0.8 GB | `innodb_buffer_pool_size=512M`、`performance_schema=OFF`、`max_connections=100` |
| C++ API 服务 + Caddy | 0.2–0.4 GB | 线程池 8–12（6C/12T），DB 连接池 ≤20；Caddy 单进程 |
| 备份/监控脚本 | 0.1 GB 峰值 | 计划任务短时运行、错峰 |
| 预留 | ≥2 GB | 否则 MySQL OOM/系统卡顿 |
> 合计约 7–9 GB 常驻 → **开服前必须先清启动项**；前端构建（Node）峰值 1–2GB 只在开发机做，生产机只放构建产物。
## 5. 备份与恢复
### 5.1 数据库
```powershell
# 每日 02:00：一致性逻辑备份（InnoDB --single-transaction）+ 7z AES-256 加密
& "C:\Program Files\MySQL\MySQL Server 8.4\bin\mysqldump.exe" --defaults-extra-file=D:\ops\backup.cnf `
  --single-transaction --quick --routines --triggers --events --set-gtid-purged=OFF lostfound |
  & "C:\Program Files\7-Zip\7z.exe" a -si -t7z -mhe=on -p"$env:BK_PASS" D:\backup\db_$(Get-Date -f yyyyMMdd).sql.7z
# binlog 增量（PITR）：my.ini 开 log_bin + binlog_expire_logs_seconds=1209600，每 15 分钟归档
```
- **不选 XtraBackup**：8.0 版本 2026-06 已 EOL，且 Percona 只提供 Linux 包（无官方 Windows 二进制，[docs](https://docs.percona.com/percona-xtrabackup/8.0/)）；本项目数据量小（<5GB），mysqldump + binlog 足够。备份账号最小权限：`SELECT, LOCK TABLES, SHOW VIEW, EVENT, TRIGGER, PROCESS`；备份文件加密 + ACL 仅管理员；**绝不把明文 dump/照片放网盘**。
- 恢复演练（每月）：建 `lostfound_restore` 库 → 解压导入 → 校验行数/最新时间戳/随机 3 张图片可打开 → 记录耗时。
### 5.2 图片、配置与保留策略
- 图片每小时 `robocopy D:\...\uploads E:\backup\uploads /MIR /R:2 /W:5` 到**另一块物理盘**（同盘不算备份）；每周加密后拷离线移动硬盘/异机；配置与证书一起备份（`Caddyfile`、`my.ini`、`ops\*.ps1`、Caddy 数据目录含内部 CA），**根证书私钥单独离线保存**。
- 保留：日备 7 份 / 周备 4 份 / 月备 12 份；图片随招领信息生命周期（§7）。**RPO/RTO 建议**：DB RPO ≤15 分钟（binlog）、≤24 小时（仅日备）；图片 RPO ≤1 小时；**RTO ≤2 小时**（恢复+启动+验证）。认领记录丢失会引发纠纷，至少做到 1 小时 RPO。
## 6. 安全清单（局域网威胁模型 → 控制项）
**威胁模型**：同 Wi-Fi 学生/攻击者（扫描、ARP 欺骗、弱口令爆破、越权遍历）、上传 WebShell/图片马、SQLi、XSS/CSRF、会话劫持、管理员账号被盗、笔记本丢失。**边界原则**：只暴露 Caddy 8443，应用与 DB 只监听 127.0.0.1。

| 控制项 | 具体要求 |
|---|---|
| 认证与会话 | **服务端会话 + Cookie**：`__Host-lf_sid`，HttpOnly + Secure + SameSite=Lax + Path=/；登录后 regenerate；空闲 30min/绝对 12h 过期；登出即失效。**不选 JWT**：无法即时吊销、泄露后有效期内一直可用、SPA 存 localStorage 更易被 XSS 窃取 |
| CSRF/CORS | SameSite=Lax + 双提交 CSRF token（所有非 GET）+ 校验 Origin/Referer；**WebSocket 握手也校验 Origin**（防 CWSH）。CORS 同源部署最省事；开发期仅放行 `http://localhost:5173`，生产禁止 `*` + credentials |
| 密码哈希 | **Argon2id**（libsodium 1.0.22 `crypto_pwhash_str`，建议 m=32MiB、t=3、p=1；OWASP 最低 19MiB/t=2）；登录渐进延迟 + IP/账号限流（5 次/分/IP、10 次/时/账号）+ 锁定；不选 bcrypt（72 字节截断、C++ 生态弱）、不选裸 SHA |
| 上传安全 | 白名单 jpg/png/webp + **magic bytes 校验**；≤8MB；**服务端重编码**（libvips/ImageMagick）去 EXIF（含 GPS）与图片马 payload；随机 UUID 文件名；存 webroot 外；经鉴权接口下发并加 `X-Content-Type-Options: nosniff`；禁 SVG/HTML；目录不可执行、无列目录、URL 不可枚举（UUID 而非自增 ID） |
| 越权/SQLi | 每个对象读写都做 owner/admin 校验，单测覆盖"改他人 id"；全量 prepared statement（MySQL Connector/C++ 或 mysqlclient 预编译），禁拼 SQL，错误不回显 SQL |
| 限流/加固 | 应用层令牌桶（IP+用户+接口），登录/上传/搜索重点限流；MySQL：`bind-address=127.0.0.1`、`skip-name-resolve`、`local_infile=OFF`，删匿名用户、root 仅 localhost，应用账号仅 SELECT/INSERT/UPDATE/DELETE，执行 `mysql_secure_installation` |
| 安全响应头 | Caddy 统一注入：HSTS（HTTPS 稳定后再开，max-age 1 年）、CSP（`default-src 'self'; frame-ancestors 'none'`）、nosniff、Referrer-Policy no-referrer、Permissions-Policy 收紧摄像头/定位、隐藏 Server 版本 |
| 审计与管理员 | 登录/失败/管理员操作/发布/认领/删除全记录；应用账号对 audit_log 仅 INSERT（不可改删）；每日导出 + 留存 **≥6 个月**（网络安全法第 21 条，[全文](https://www.cac.gov.cn/2016-11/07/c_1119867116_2.htm)）；管理员独立路径 + 强口令 + TOTP、Caddy `remote_ip` 限制入口（仅本机/Tailscale 网段）、首登强制改密、密钥不入前端 bundle |
## 7. 合规（中国）

| 要求 | 依据 | 本项目落地 |
|---|---|---|
| 最小必要、目的限定 | PIPL 第 5/6 条 | 字段白名单：学号、姓名、联系方式、物品类别/描述/照片；**不采集身份证、人脸、精确位置**（上传自动剥离 EXIF 坐标） |
| 告知同意 | PIPL 第 17 条 | 注册/首次发布前展示隐私政策并勾选；记录同意时间/版本/IP；变更再告知 |
| 敏感个人信息/单独同意 | PIPL 第 28/29 条；《人脸识别技术应用安全管理办法》（2025-06-01，[全文](https://www.gov.cn/zhengce/zhengceku/202503/content_7016075.htm)） | 照片含人脸即敏感 → **产品上禁止人脸上传、提示只拍物品**；如确需人脸：单独同意 + 影响评估（报告留存 ≥3 年）+ **不得作为唯一验证方式** + 原则上不得经互联网传输；≥10 万人需备案（本项目不触及） |
| 未成年人 | PIPL 第 31 条 | 不满 14 周岁需监护人同意 + 专门处理规则；注册页声明"未满 14 岁请勿使用" |
| 脱敏展示 | PIPL 第 6 条 | 公开列表只显示 `张*`、`138****1234`、学号后 4 位；认领申请经管理员/双方确认后定向披露联系方式 |
| 保留与删除 | PIPL 第 19/47 条；《网络数据安全管理条例》第 9 条 | 招领信息 6 个月（学期末清理）、认领记录 1 年、日志 ≥6 个月、注销后 30 天内删除；到期自动清理；用户可删除发布/注销账号，30 天内处理并留痕 |
| 数据出境 | PIPL 第 38 条 | **真实数据不出境**：不用境外对象存储/CDN/隧道承载业务数据；Cloudflare Tunnel/Tailscale 仅演示或运维，不传真实个人信息 |
| 事件处置 | 《网络数据安全管理条例》第 10/11 条（[全文](https://www.gov.cn/zhengce/zhengceku/202409/content_6977767.htm)） | 泄露等事件按规定 24 小时内报告主管部门并通知利害关系人；保留处置记录 |
| 免责与报备 | 平台自律；网络安全法第 21 条（等保、日志 6 个月） | 免责声明："仅信息发布与撮合，不保管物品、不担保真实性；禁止发布违禁品/他人隐私；冒领自负法律责任；管理员有权删除"（发布须知 + 页脚双处）；上线前向学校信息化/网络安全部门**提交系统备案**（用途、域名/IP、数据字段、访问范围），按学校要求做等保（通常二级）；若映射公网或用学校域名，走学校统一 ICP 备案审批，**未经批准不对外网开放** |
## 8. 验收测试清单

| 类别 | 用例（通过标准） |
|---|---|
| 网络 | ① 同 SSID 手机打开登录页；② 路由器/重连后 IP 变化，二维码书签仍可用；③ 断开校园网后本机内网仍可访问（方案 B 验证）；④ 跨楼/异网段按预期被拒并有提示；⑤ 连续 30 分钟 WSS 不掉线（掉线自动重连成功） |
| HTTPS | ① 手机 Chrome/Safari 无证书警告；② 证书 SAN 含实际访问 IP/域名；③ 续期演练（删证书后自动重签）；④ 无混合内容（Console 无 ws:// 警告）；⑤ HTTP 请求正确 308 到 HTTPS 或不可达；⑥ Firefox 无信任告警 |
| 安全 | ① 未登录访问 API 全 401；② 改他人资源 id 越权失败（IDOR 用例集）；③ 上传 .php/.svg/改扩展名图片被拒；④ 上传图 EXIF/GPS 已剥离；⑤ 连续错误登录触发限流/锁定；⑥ SQLi 探测串返回统一错误；⑦ 响应头自查（HSTS/CSP/nosniff/X-Frame-Options）；⑧ 审计日志含登录/管理/删除且不可改 |
| 备份恢复 | ① 从最近备份恢复到新库，行数一致；② binlog 做到"删除某行后可恢复到删除前"；③ 随机 10 张图片从备份可打开；④ 恢复总耗时 ≤2h 并记录；⑤ 备份为加密包、权限仅管理员；⑥ 异机副本可独立启动（含配置/证书） |
| 运维 | ① 拔电后靠电池继续服务；② 强制重启后 5 分钟内全部服务自动恢复并告警；③ 合盖/关屏不影响访问；④ kill 进程 10 秒内自动拉起；⑤ Windows Update 重启后自检通过；⑥ 远程 SSH 仅从 Tailscale 网段可连 |
## 9. 风险 → Fallback 速查与主要来源

| 风险 | Fallback |
|---|---|
| §1.3 实测被 AP 隔离 | 方案 B 自带路由器；或 Tailscale 兜底 |
| 80/443 被校园网封 | 对外用 8443/9443；证书走 DNS-01（不依赖入站） |
| 无域名预算/无法改 DNS | Caddy `tls internal` + 各设备装根证书（§2.4） |
| 校园网 DHCP 频繁换 IP | 自带路由器 LAN DHCP 保留 / 向网络中心申请固定 IP |
| Insider 重启 / 断电 / 内存不足 / 合规争议 | 退出 Insider + 服务自启 + 健康检查；电池+UPS；清启动项降 buffer pool；立即下线人脸功能、脱敏、补齐隐私政策与报备材料 |

**主要来源**：Caddy [Automatic HTTPS](https://caddyserver.com/docs/automatic-https)｜[Keep Caddy Running](https://caddyserver.com/docs/running)｜[tls internal](https://caddyserver.com/docs/caddyfile/directives/tls)｜[reverse_proxy（重载断 WS）](https://caddyserver.com/docs/caddyfile/directives/reverse_proxy)；[mkcert](https://github.com/FiloSottile/mkcert)；[Let's Encrypt 挑战类型](https://letsencrypt.org/docs/challenge-types/)；[Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/)；[Tailscale HTTPS](https://tailscale.com/docs/how-to/set-up-https-certificates)；[WinSW](https://github.com/winsw/winsw)；[NSSM](https://nssm.cc/download)；[MySQL EOL](https://endoflife.date/mysql)；[XtraBackup](https://docs.percona.com/percona-xtrabackup/8.0/)；[OWASP 密码存储](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)；[libsodium](https://github.com/jedisct1/libsodium)；中国法规：[网络安全法](https://www.cac.gov.cn/2016-11/07/c_1119867116_2.htm)、[网络数据安全管理条例](https://www.gov.cn/zhengce/zhengceku/202409/content_6977767.htm)、[人脸识别办法](https://www.gov.cn/zhengce/zhengceku/202503/content_7016075.htm)、[PIPL](https://flk.npc.gov.cn/detail?title=%E4%B8%AD%E5%8D%8E%E4%BA%BA%E6%B0%91%E5%85%B1%E5%92%8C%E5%9B%BD%E4%B8%AA%E4%BA%BA%E4%BF%A1%E6%81%AF%E4%BF%9D%E6%8A%A4%E6%B3%95&id=ff8081817b6472a3017b656cc2040044)。
