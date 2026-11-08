# 前端架构 / 实时交互 / 移动端 Web 约束 调研（基准日 2026-11-08）

> 范围：校园失物招领系统（C++ 后端、MySQL、局域网单源、手机+电脑响应式、拍照上传、实时通知）。
> 方法：npm registry dist-tags、各框架官方发布博客、MDN/W3C/WHATWG、OWASP、caniuse 数据，均于 2026-11-08 核实。
> 依赖前置：本机无 Node/pnpm（见 environment-audit.md），前端工具链需 M0 安装；HTTP/证书方案见 lan-security-ops.md（本文件只给前端功能影响）。

## 0. TL;DR（可直接拍板的结论）

1. **选 SPA，不选 SSR**：Vite 8.3.6 + React 19.3.1 + TypeScript 6.0.3；生产构建产物由 Caddy 托管静态文件，`/api` 与 `/ws` 反代到 C++ 服务（同源，免 CORS，cookie 直通）。
2. **实时用原生 WebSocket + REST，不用 socket.io**（socket.io 官方 server 实现列表中没有 C++）；WS 只做"有新事件"信号，数据一律走 REST 后由 TanStack Query 刷新。
3. **secure context 是功能分水岭**：`http://192.168.1.100` 下 getUserMedia / Service Worker / PWA 安装 / Notification 全部不可用；**`<input type="file" accept="image/*" capture>` 拍照可用**，这是 M1 阶段（无 HTTPS）的唯一拍照路径。
4. 图片：客户端 canvas 压缩为 WebP 长边 1600 / q0.8，`accept` 排除 HEIC；multipart 上传 + XHR 进度 + 幂等重试。
5. Web Push 放到阶段 3（HTTPS+PWA 稳定且用户有"关页面也要收通知"的明确需求后）；C++ 侧实现 VAPID/aes128gcm 成本高，MVP 不做。

## 1. 前端选型矩阵与 SPA/SSR 决策（2026-11 实测版本）

| 方案 | 2026-11 稳定版本 | 状态/关键事实 | 本项目结论 |
|---|---|---|---|
| **React + Vite（SPA）** | react 19.3.1（19.3 发布于 2026-09-09，View Transitions/Fragment Ref 转正）、vite 8.3.6（Vite 8.0 于 2026-03-12 发布，Rolldown 单打包器） | 生态最全：shadcn/TanStack/RHF 全是 React 优先 | **推荐** |
| Next.js 16（RSC/SSR） | next 16.4.5（16.3 于 2026-08-03） | 需 Node 常驻进程；RSC/缓存/自托管复杂度高 | 否决：无 SEO 需求、单源局域网、本机空闲内存仅 3GB，多一个 Node 常驻进程不划算 |
| Nuxt 4（SSR） | nuxt 4.6.4 | Vue 稳定版仍为 3.5.45，3.6 只有 RC（3.6.0-rc.14，2026-10-30） | 否决：SSR 无收益；Vapor/3.6 未稳定 |
| SvelteKit 3 | @sveltejs/kit 3.1.1（3.0 于 2026-10-01 发布） | 要求 Node>=22.17、Vite^8、Svelte^5.57、TS^6；轻量、体验好 | 否决：移动端组件/PWA/表单生态与招人面明显小于 React，SSR 能力用不上 |
| SolidStart | @solidjs/start 2.2.2（solid-js 1.9.18） | 性能优秀但生态/踩坑少、社区小 | 否决：小团队无人可互助，风险不对称 |
| Vue 3 + Vite（SPA，备选） | vue 3.5.45 + vue-router 5.4.0 + pinia 4.1.0 + vant 4.10.2 | 若团队 Vue 更熟，等价可行 | 备选：不采用 SSR 时与 React 方案收益对等 |

**选 SPA 的 6 条硬理由**：① 局域网单源、无 SEO/首屏服务端渲染需求；② 静态产物可由 Caddy 直接托管，C++ 服务只做 API，不新增常驻运行时；③ PWA/离线缓存/WS 在 SPA 下实现最简单；④ 部署=拷 dist 目录，回滚=换目录，符合"本机 Windows 单机运维"；⑤ 调试链路短（浏览器 → Caddy → C++），本机内存紧张时少一个进程；⑥ React 19.3 的 View Transitions 已稳定，路由过渡不需要 SSR 框架。
**明确否决 SSR 的场景性理由**：首屏走本机内网（RTT 1–3ms），SSR 的"减少白屏"收益趋近于 0；而 SSR 带来的 Node 进程、缓存/水合不一致、HTTPS/域名配置（SSR 也需要安全上下文才能用 PWA）成本全部保留。

**运行时拓扑（前后端契约）**：Caddy(:443 或 :8443) → `/` 静态 dist + SPA history fallback；`/api/*` → C++ :8080；`/ws` → C++ :8080（Upgrade 直通）；`/uploads/*` 由 Caddy 直接给文件或反代 C++。所有前端请求用相对路径 `/api/...`，禁止硬编码 IP（DHCP 会变，见 §4 坑）。

## 2. 配套选型与版本兼容性（npm peer 实测，2026-11-08）

| 用途 | 选型与版本 | 兼容性证据/说明 | 被否决项与理由 |
|---|---|---|---|
| 构建 | vite 8.3.6 + @vitejs/plugin-react 6.2.1 | plugin-react peer `vite ^8.0.0`；Vite 8 要求 Node `^20.19 \|\| >=22.12` | webpack/Rspack：配置成本高、无收益 |
| 语言 | typescript 6.0.3 | TS 7.0.2 已稳定（2026-07-08 原生版）但 **typescript-eslint 8.72.1 的 peer 为 `>=4.8.4 <6.1.0`**，TS7 暂时无法走常规 lint 链 | 先用 6.0.3，待 lint 工具支持后升级 7（tsc 7 可只做类型检查提速） |
| 样式 | tailwindcss 4.3.5 + @tailwindcss/vite 4.3.5 | peer `vite ^5.2 \|\| ^6 \|\| ^7 \|\| ^8`；v4.3 于 2026-05-08 发布 | CSS-in-JS（运行时开销）、UnoCSS（团队熟悉度低） |
| 组件 | **shadcn/ui（Radix 基座）+ lucide-react 1.70.0** | shadcn 2026-07 起默认 Base UI，但 `@base-ui-components/react` 最新仅 **1.0.0-rc.0**（未 GA）；`radix-ui` 1.8.1 稳定，shadcn 同时提供 Radix/Base UI/React Aria 三套 | Ant Design Mobile 5.43.0（2026-09-14 仍在维护，表单密集场景可用；但包体与视觉风格绑定强，首屏预算难控） |
| 路由 | **TanStack Router 1.170.46** | peer react >=18/19，Node >=20.19；类型安全 params/search、zod 校验 search、内置 loader/prefetch | React Router 8.5.1（非破坏升级自 v7，Node22+/React19+/ESM-only，也可用；若团队只会 RR 则选它）；React Router 7 已过基线 |
| 服务端状态 | **TanStack Query 5.107.1** | peer react ^18\|^19；v5 线持续更新（无 v6） | SWR（mutation/离线/失效控制较弱）、Redux Toolkit Query（过重） |
| 客户端状态 | **zustand 5.1.0** | peer react >=18；只放 UI 状态（筛选草稿、WS 状态、未读 badge） | Redux（样板代码多）、Jotai（本项目收益不明显） |
| 表单/校验 | **react-hook-form 7.91.0 + zod 4.6.7 + @hookform/resolvers 7.0.3** | resolvers peer `zod ^3.25 \|\| ^4`、`react-hook-form ^7.55`；RHF peer react 16.8–19 | Formik（维护弱）、TanStack Form（新，团队学习成本） |
| PWA | **vite-plugin-pwa 2.0.0 + workbox-window 7.4.1** | peer `vite ^3–^8`，Node>=20.19（支持 Vite 8） | 手写 SW（缓存版本控制易错） |
| 长列表 | @tanstack/react-virtual 3.14.26 | peer react 16.8–19 | 全量渲染（>50 条掉帧） |
| 编辑器/质量 | eslint 10.14.0 + typescript-eslint 8.72.1（配 TS 6.0.3） | 见上 | — |
| 运行时 | **Node.js 26.13.0 LTS**（Active LTS，2026-10-28 起；备选 24.22.2 维护版）+ **pnpm 12.18.0** | Node 26 已 Enter LTS，支持到 2029-04；不得使用 Node 20（2026-04 EOL） | npm 亦可；corepack 固定 pnpm 版本 |

### 2.1 工程初始化与版本锁定（可直接执行，pnpm -E 锁精确版本）

```bash
node -v    # 期望 v26.13.0 LTS（或 v24.22.2）；pnpm -v 期望 12.18.0
pnpm create vite@latest web --template react-ts
cd web
pnpm add -E react@19.3.1 react-dom@19.3.1 @tanstack/react-router@1.170.46 \
  @tanstack/react-query@5.107.1 zustand@5.1.0 react-hook-form@7.91.0 \
  @hookform/resolvers@7.0.3 zod@4.6.7 lucide-react@1.70.0
pnpm add -D -E vite@8.3.6 @vitejs/plugin-react@6.2.1 tailwindcss@4.3.5 @tailwindcss/vite@4.3.5 \
  typescript@6.0.3 vite-plugin-pwa@2.0.0 workbox-window@7.4.1 @tanstack/react-virtual@3.14.26
pnpm dlx shadcn@latest init     # 基座选 Radix（Base UI 仍 1.0.0-rc.0）
```

上线前必须人工确认的两行（浏览器控制台，手机/PC 各一次）：`window.isSecureContext`（决定能否用 SW/getUserMedia/Notification）与 `navigator.mediaDevices !== undefined`（决定相机方案），结果写入验收记录。


## 3. 实时方案取舍：原生 WebSocket + REST（Q3）

| 方案 | 结论 | 理由 |
|---|---|---|
| 轮询（15–30s） | **仅兜底** | 实现最简，但通知延迟高、请求放大；WS 连续失败 3 次后启用，仅页面可见时轮询 |
| SSE（EventSource） | 备选通道 | HTTP/1.1 下每浏览器+域名仅 **6 条连接**（HTTP/2 才缓解，MDN），且无双向；但自带重连与 `Last-Event-ID` |
| **原生 WebSocket** | **主方案** | C++ 侧 uWebSockets/libwebsockets/Beast/Drogon 原生支持；浏览器 API 就是 RFC6455，无协议层；可 ping/pong、低延迟 |
| socket.io | **否决** | 官方"Server implementations"列表只有 JS/Deno/Java/Python/Go/Rust，**无 C++ server**；且官方明示"Socket.IO is NOT a WebSocket implementation"（Engine.IO 封装），C++ 侧要自己实现协议=不可控工作量 |
| WebTransport | 不在 MVP | 浏览器支持与 C++ 服务端库都更窄，收益不明显 |

**协议契约（给 C++ 同事）**：同源 `wss://<host>/ws`（HTTP 阶段为 `ws://`）；握手带 Cookie；首帧服务端下发 `{"type":"hello","lastEventId":N}`；业务消息只含 `{eventId, type:"new_post"|"new_claim"|"chat", payload精简}`；客户端 25s 发 `ping`，服务端 `pong`，10s 未收即判死；服务端对 60s 无数据连接清理。
**鉴权/防 CSWSH（OWASP WebSocket Cheat Sheet）**：HttpOnly+SameSite=Lax 会话 cookie；**服务端对每个 upgrade 校验 Origin 白名单**（精确匹配，禁通配/子串）；每 30 分钟重校验会话，登出立即关闭该用户全部连接；不用 URL 携带 token（会进日志），如必须传 token 则首帧消息传并在校验前不下发业务数据。
**重连与补偿**：指数退避 0.5→1→2→4→8→16→30s，±20% 抖动，连接稳定 30s 后重置；`online`/页面可见/从 bfcache 恢复立即触发一次；事件表用自增 `id`，客户端保存 `lastEventId`，重连后 `GET /api/events?since=<id>&limit=50` 补齐（SSE 用 `Last-Event-ID`，WS 用 query/first-frame），按 id 去重；发现 id 跳号→主动全量拉当前页。REST 列表也用 `since`/`cursor` 分页，保证可补偿。
**移动端后台挂起恢复**：监听 `visibilitychange`、`pageshow.persisted`、`document.freeze/resume`（WICG Page Lifecycle，`wasDiscarded`）、`online`；**不要假设 WS 一直活着**——visibility=visible 时：① WS 非 OPEN 就重连；② 并发拉 `/api/notifications/unread` 与 `since` 增量；③ 用 unread 数刷新 badge。iOS 后台会冻结/回收页面，恢复后必须走"先拉后订阅"。
**Web Push / PWA 推送（2026-11 状态）**：Android Chrome（caniuse 快照 Chrome 157）Web Push 正常（FCM）；iOS/iPadOS 自 16.4 起支持，但**必须先从分享菜单"添加到主屏幕"并在独立窗口内授权**（canIuse 备注 #7；Safari 标签页内无 Notification 权限）；两者都要求 HTTPS+SW+VAPID。**阶段 3 再做的判据**：HTTPS 已稳定上线 + A2HS 安装率>20% + 有"关页面仍需通知"的真实需求；否则性价比低（C++ 侧要自行实现 VAPID ES256 JWT + aes128gcm 加密，约 2–4 人日且易错）。

## 4. Secure Context 核实（重点，决定方案分叉）

**为什么 `http://192.168.1.100` 不是安全上下文**：安全上下文 = 文档 URL 是"potentially trustworthy"；MDN 判定表只认 `https://`、`http://localhost`/`127.0.0.1`、`file://`（W3C Secure Contexts 同样只把 127.0.0.0/8、::1、localhost、https/wss 列入）。**私有 IP 字面量（192.168.x.x / 10.x.x.x）不在其中**，所以 `window.isSecureContext === false`，浏览器不会把它当安全上下文。

| 能力 | HTTP（如 http://192.168.1.100:8443） | HTTPS（受信证书） | 影响/兜底 |
|---|---|---|---|
| `navigator.mediaDevices.getUserMedia`（自建相机） | **不可用**（MDN 标注 Secure context；Chrome 下 mediaDevices 为 undefined） | 可用（需用户授权） | HTTP 阶段改用 `<input capture>` 调系统相机 |
| Service Worker（离线/缓存/推送前提） | **不可用** | 可用 | HTTP 阶段无离线缓存，只能浏览器 HTTP 缓存 |
| PWA 安装（manifest+可安装性） | **不可安装**（MDN：必须 https 或 localhost，比 secure context 更严） | 可安装 | 无"添加到主屏幕"引导 |
| Notification 权限 / Web Push | **不可用** | 可用（iOS 还需 A2HS） | 站内消息 + badge 轮询 |
| `<input type="file" accept="image/*" capture>` **拍照上传** | **可用** ✅ | 可用 | **HTTP 阶段唯一拍照路径**；见下方查证 |
| WebSocket（`ws://`） | 可用（WS 不受 secure context 约束） | 必须 `wss://`（HTTPS 页面禁止 ws:// 混合内容） | 升级 HTTPS 时同时换 wss |

**`capture` 在 HTTP 下能否用（必须查证清楚）→ 能**。依据：① 规范层面 `capture` 是 W3C HTML Media Capture 对 `<input type=file>` 的扩展（2017 PR，全文无 `secure` 字样、无 [SecureContext] 门禁），它只是"请求系统拍照 UI"的声明式提示；② MDN `capture` 属性页只在 file input 上生效，无 "Secure context" 警告（对比 getUserMedia 页有明确警告）；③ 行为是 UA 调用系统相机/相册，不经过 Web 权限 API。实测差异：**iOS Safari 无论 accept 写什么都会把照片转成 JPEG**；**macOS Safari 只要 accept 不含 image/heic 就按 accept 第一个可转换格式转换（否则保留 HEIC）**；Android Chrome 会直接进相机或给"相机/文件"选择器（取决于机型/OEM）。→ 结论：**M1（HTTP）阶段用 `<input type="file" accept="image/jpeg,image/png,image/webp" capture="environment">` 实现拍照**，不做 getUserMedia 自建相机；同时设计"上传按钮 = 拍照 + 从相册选"两个入口。

**iOS vs Android 差异（影响验收与文案）**：① 安装：iOS 16.4+ 可在 Safari/Chrome/Edge/Firefox/Orion 的分享菜单"添加到主屏幕"（16.3- 仅 Safari）；Android 只有 Chrome(GMS)/三星浏览器装成 WebAPK，其他浏览器只是快捷方式（MDN）；Safari 从不触发 `beforeinstallprompt`，iOS 必须给图文引导。② 通知：Android 用系统通知；iOS 仅 A2HS 后可用、且标签页内无 Notification。③ 相机：iOS 的 `capture` 直接唤起系统相机；Android 可能先弹"相机/文件"选择。④ 所有 iOS 浏览器内核都是 WebKit，功能差异按 iOS 版本而非浏览器品牌判断。

**两个隐藏坑**：① origin 含 IP：DHCP 换 IP 后 localStorage/cookie/SW 缓存全部作废 → 用固定主机名（DHCP 保留/路由器绑定）再谈 PWA；② 自签证书未装到手机上时，SW 注册与安装同样失败（需把根 CA 导入手机，见 lan-security-ops.md）。

**功能开关（前端按 `window.isSecureContext` + `location.protocol` 降级）**：`capabilities = { pwa: sw && https, camera: https ? "getUserMedia" : "file-input", push: sw && https, install: sw && https }`；HTTP 阶段隐藏"安装到桌面/开启通知/离线"入口，避免用户点了报错。

## 5. 图片上传方案（Q5）

**流水线**：选图/拍照 → 校验（type ∈ jpeg/png/webp、size≤5MB）→（>300KB 或长边>1600 才压缩）`createImageBitmap(file,{imageOrientation:"from-image"})`（默认按 EXIF 摆正）→ 离屏 canvas 缩到长边 1600 → `toBlob("image/webp",0.8)`，**校验 `blob.type`，非 webp（Safari 不支持编码时回落 PNG）则改用 `"image/jpeg",0.82`**（MDN：不支持的类型会导出 PNG）→ 目标 ≤400KB → `FormData` multipart POST `/api/uploads`（字段 file + `Idempotency-Key`）→ 服务端返回 `{url,thumbUrl,width,height}`。
**HEIC**：前端在 `accept` 中**排除 image/heic**，让 macOS Safari 转 JPEG（iOS 必转 JPEG）；Windows Chrome/Firefox 无法解码 HEIC，检测到 HEIC 就提示"请用手机拍照或先转 JPEG"，**不引入 heic2any**（~1MB WASM，收益不抵成本；服务端如需要可用 libheif 兜底，非 MVP）。
**进度/重试**：用 **XHR**（`xhr.upload.onprogress`；fetch 无标准上传进度），per-file 状态机 queued→compressing→uploading→done/failed，支持取消（`xhr.abort()`）；失败退避重试 2 次（1s/3s），幂等键防重复入库；并发 2；离线时暂存内存队列并提示，不做分片断点续传（单图≤400KB 无必要）。
**隐私/安全**：客户端重编码天然剥离 EXIF（含 GPS）；服务端仍须校验 magic bytes、尺寸上限、随机文件名、按 `YYYY/MM/` 落盘，禁止执行权限；缩略图（长边 640）由服务端生成（C++ 侧 libvips/OpenCV，若后端不做则前端多传一张 thumb——需与 task-1 对齐）。
**被否决**：原图直传（流量/存储翻 5–10 倍，10 万张 2MB≈200GB，见环境报告）、base64 内联 JSON（+33% 体积、内存峰值）、多图打包 zip（无预览/重试粒度差）。

## 6. "丝滑"落地清单与可量化验收（Q6）

**清单（按收益排序）**：① 骨架屏：列表/详情用与真实布局同尺寸的 skeleton，图片预留 `aspect-ratio`，**CLS 目标 0 抖动**；② 乐观更新：认领/收藏/"已读"用 TanStack Query `onMutate` 先改缓存、失败回滚，操作反馈 <100ms；③ 虚拟化：列表 >50 条用 @tanstack/react-virtual，滚动保持 55fps+；④ 图片懒加载：缩略图 + `loading="lazy"`/`decoding="async"`，IntersectionObserver 提前 300px 预取，占位用纯色/模糊；⑤ 路由过渡：React 19.3 `<ViewTransition>` + `startTransition`（原生 View Transitions 在 Chrome 157/iOS Safari 27 均支持），并尊重 `prefers-reduced-motion`；⑥ 离线壳：SW precache app shell + 图片 CacheFirst（上限 LRU 100 张），断网可看缓存详情；⑦ 弱网：请求超时 10s、失败重试、顶部离线横幅、提交按钮防重、WS 断开降级轮询；⑧ 移动细节：点按区≥44px、`env(safe-area-inset-*)`、`100dvh`、输入框字号≥16px 防缩放、列表滚动位置恢复、hover/idle 预取详情。
**验收指标（p75，局域网，中端 Android/iPhone 实测）**：LCP ≤2.5s、INP ≤200ms、CLS ≤0.1（web-vitals 官方 good 阈值 [0.1/200/2500]）；首屏 JS gzip ≤250KB、单路由 chunk ≤120KB；列表滚动 ≥55fps；图片占位→显示 P95 ≤400ms；WS 通知到 UI P95 ≤1s；断线重连成功 ≤3s 且成功率 ≥99%；拍照→本地预览 ≤1s；1600px 照片压缩后 ≤400KB 且上传 P95 ≤3s；离线二次打开可用（HTTPS 阶段）；Lighthouse PWA installable 全绿（HTTPS 阶段）；控制台 0 error。
**测量**：引入 `web-vitals` 上报到自建 `/api/metrics`（不接外部统计，校园内网可用）；CI 用 Lighthouse（M0 先人工跑 3 台真机）。

## 7. 风险与 fallback

| 风险 | 影响 | 对策/fallback |
|---|---|---|
| 校园网 AP 隔离/客户端隔离（环境报告已标红） | 手机连不上本机，整个方案不成立（前端无解） | 先做 M0 可达性实测；兜底：手机热点/自建路由器，或内网穿透（见 lan-security-ops.md） |
| 无法给手机装可信证书 | 无 HTTPS → 无 PWA/SW/getUserMedia/推送 | 功能降级开关（§4）；拍照走 capture；仅保留 WS+REST |
| 笔记本休眠/断电 | WS 全断、服务不可用 | 电源策略（见环境报告）+ 客户端全量重连/校准；告知用户"服务不可用"页 |
| Base UI 仍 RC（1.0.0-rc.0）而 shadcn 默认它 | 升级踩坑 | MVP 用 Radix（radix-ui 1.8.1），锁定 shadcn 生成版本，不追 latest |
| TS 7 已 GA 但 lint 链未跟上 | 类型检查可跑、ESLint 报错 | 锁 TS 6.0.3；tsc7 仅作类型检查实验 |
| Tailwind Labs 2026-09 并入 Shopify | 治理变动带来路线风险 | 锁 4.3.5；Tailwind 是编译期工具，替换成本低于运行时框架 |
| 大图/多图存储增长 | 磁盘 90GB 可用 | 压缩+缩略图+定期归档；服务端限 3 张/条 |

## 8. 来源（2026-11-08 核实）

1. React 19.3（2026-09-09）https://react.dev/blog/2026/09/09/react-19-3 ｜ 版本 https://registry.npmjs.org/react/latest
2. Vite 8.0（2026-03-12，Rolldown）https://vite.dev/blog/announcing-vite8.html ｜ https://registry.npmjs.org/vite/latest
3. Next.js 16.3（2026-08-03）https://nextjs.org/blog/next-16-3 ｜ https://registry.npmjs.org/next/latest
4. SvelteKit 3（2026-10-01）https://svelte.dev/blog ｜ https://registry.npmjs.org/@sveltejs%2Fkit/latest
5. Vue/Nuxt/SolidStart 版本与 RC 状态 https://registry.npmjs.org/-/package/vue/dist-tags ｜ https://registry.npmjs.org/-/package/nuxt/latest ｜ https://registry.npmjs.org/-/package/@solidjs%2Fstart/latest
6. Node LTS 计划 https://raw.githubusercontent.com/nodejs/Release/main/schedule.json ｜ Node 26.13.0 / 24.22.2 https://nodejs.org/dist/latest-v26.x/SHASUMS256.txt ｜ pnpm https://registry.npmjs.org/-/package/pnpm/dist-tags
7. shadcn 更新日志（2026-07 Base UI as the Default）https://ui.shadcn.com/docs/changelog ｜ Base UI RC https://registry.npmjs.org/-/package/@base-ui-components%2Freact/dist-tags ｜ Radix 1.8.1 https://registry.npmjs.org/-/package/radix-ui/dist-tags
8. Tailwind v4.3（2026-05-08）与 Tailwind Labs 加入 Shopify（2026-09-09）https://tailwindcss.com/blog
9. TypeScript 7.0（2026-07-08）/7.1 Beta（2026-10-20）https://devblogs.microsoft.com/typescript/announcing-typescript-7-0/ ｜ typescript-eslint peer https://registry.npmjs.org/typescript-eslint/latest
10. React Router v8 基线 https://reactrouter.com/ ｜ TanStack 版本 https://registry.npmjs.org/@tanstack%2Freact-router/latest
11. Socket.IO 官方 server 实现清单（无 C++）与"非 WebSocket 实现" https://socket.io/docs/v4/
12. OWASP WebSocket 安全（Origin 白名单/CSWSH/SameSite/30 分钟重校验）https://cheatsheetseries.owasp.org/cheatsheets/WebSocket_Security_Cheat_Sheet.html
13. SSE 连接上限与 Last-Event-ID https://developer.mozilla.org/en-US/docs/Web/API/EventSource
14. Page Lifecycle（freeze/resume/wasDiscarded）https://wicg.github.io/page-lifecycle/
15. Secure Contexts 定义/判定 https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts ｜ https://w3c.github.io/webappsec-secure-contexts/
16. getUserMedia 需安全上下文 https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia ｜ Service Worker https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API ｜ Notification https://developer.mozilla.org/en-US/docs/Web/API/Notification
17. PWA 可安装性（HTTPS 必需、iOS/Android 安装差异）https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/Guides/Making_PWAs_installable ｜ beforeinstallprompt（仅 Chromium 系）https://developer.mozilla.org/en-US/docs/Web/API/Window/beforeinstallprompt_event
18. capture 属性与 HTML Media Capture 规范 https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Attributes/capture ｜ https://w3c.github.io/html-media-capture/
19. iOS 16.4 Web Push（仅 A2HS）https://webkit.org/blog/13966/webkit-features-in-safari-16-4/ ｜ caniuse Push API 备注 #7 https://raw.githubusercontent.com/Fyrd/caniuse/main/features-json/push-api.json
20. Safari file input 的 HEIC 转换实测 https://raw.githubusercontent.com/koki-develop/zenn-contents/main/articles/safari-input-file-heic.md
21. createImageBitmap imageOrientation https://developer.mozilla.org/en-US/docs/Web/API/Window/createImageBitmap ｜ toBlob 类型回落 https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/toBlob ｜ XHR 上传进度 https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest/upload
22. Core Web Vitals 阈值（LCP 2500/INP 200/CLS 0.1）https://raw.githubusercontent.com/GoogleChrome/web-vitals/main/README.md
23. 浏览器基线（Chrome 157 / iOS Safari 27.2、View Transitions、WebP）https://raw.githubusercontent.com/Fyrd/caniuse/main/features-json/view-transitions.json