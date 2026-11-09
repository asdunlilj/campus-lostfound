# 校园失物招领系统 · 前端/后端接口契约（v1）

> 服务地址：http://<host>:8080 ；静态单页应用由服务端在 GET / 返回。
> 认证：登录成功下发 Cookie lf_sid（HttpOnly + SameSite=Lax），浏览器同源请求自动携带。
> 所有非 GET 请求必须带请求头 X-Requested-With: CampusLF（CSRF 防护），否则返回 403。
> 所有响应均为 UTF-8 JSON（SSE 与图片除外）。

## 0. 统一响应格式

成功：
~~~json
{ "ok": true, "data": { } }
~~~
失败：
~~~json
{ "ok": false, "error": { "code": "INVALID_PARAM", "message": "标题不能为空" } }
~~~
错误码：INVALID_PARAM / UNAUTHORIZED / FORBIDDEN / NOT_FOUND / CONFLICT / RATE_LIMITED / INTERNAL。
HTTP 状态码与错误对应：400/401/403/404/409/429/500。

## 1. 视图对象

### 1.1 UserView
~~~json
{ "id": 1, "studentNo": "20230001", "name": "张三", "college": "计算机学院",
  "role": "student", "createdAt": "2026-11-08T02:30:00Z" }
~~~
说明：公开给别人看的 name 显示为脱敏（张*）；自己看自己返回全名。管理员看别人返回全名。

### 1.2 ItemView（列表用）
~~~json
{ "publicId": "a1b2c3d4e5f6a7b8c9d0e1f2", "type": 1, "typeText": "招领",
  "title": "捡到一张校园卡", "description": "在图书馆二楼捡到，卡面姓名王*",
  "categoryId": 1, "categoryName": "证件卡类",
  "locationId": 2, "locationName": "图书馆", "locationBuilding": "",
  "eventTime": "2026-11-08 14:30", "status": 0, "statusText": "进行中",
  "viewCount": 12, "createdAt": "2026-11-08T06:30:00Z", "createdAtText": "2 分钟前",
  "expiresAt": "2026-12-08T06:30:00Z",
  "owner": { "name": "张*", "college": "计算机学院" },
  "images": [ { "id": "img1", "url": "/media/20261108_abc123.jpg" } ],
  "verifyQuestion": "卡背面写了什么字？",
  "claimCount": 1, "canManage": false, "claimAllowed": true }
~~~
说明：canManage 表示当前用户可编辑/下架/审核该物品（发布者或管理员）；claimAllowed 表示当前用户可提交认领（已登录、非发布者、status=0 且自己没有进行中的认领）。
详情页与列表页字段一致。

### 1.3 ClaimView
~~~json
{ "id": 3, "itemPublicId": "a1b2...", "itemTitle": "捡到一张校园卡", "itemType": 1,
  "claimantName": "李*", "status": 0, "statusText": "待审核",
  "answer": "卡背面写了 2023", "message": "应该是我的，谢谢！",
  "createdAt": "2026-11-08T07:00:00Z", "createdAtText": "10 分钟前",
  "reviewedAt": "", "completedAt": "", "pickupExpiresAt": "",
  "pickupCode": "",
  "canReview": false, "canCancel": false, "canVerify": false }
~~~
说明：pickupCode 只在“当前用户是认领人且 status 为 1/5”时返回；canReview 表示当前用户可审批（物品发布者或管理员）；canVerify 表示可核销（物品发布者或管理员，status=1）。

### 1.4 NotificationView
~~~json
{ "id": 5, "type": "claim", "title": "收到新的认领申请",
  "body": "「捡到一张校园卡」收到认领申请", "link": "#/item/a1b2...",
  "read": false, "createdAt": "2026-11-08T07:00:00Z", "createdAtText": "10 分钟前" }
~~~

### 1.5 ReportView
~~~json
{ "id": 2, "targetType": "item", "itemPublicId": "a1b2...", "itemTitle": "捡到一张校园卡",
  "reason": "信息不实", "status": 0, "statusText": "待处理",
  "reporterName": "李*", "createdAt": "...", "createdAtText": "..." }
~~~

### 1.6 InviteView
~~~json
{ "codeMasked": "LF-3F9A****", "maxUses": 500, "uses": 3, "expiresAt": "2027-11-08T00:00:00Z",
  "expiresAtText": "2027-11-08 08:00", "disabled": false,
  "createdBy": "admin", "createdAt": "..." }
~~~
注意：列表/查询接口**只返回掩码**；完整邀请码仅在「创建邀请码」响应里返回一次（字段 code），
服务器数据库只保存 SHA-256 哈希，无法再次取回明文。

### 1.7 AuditView
~~~json
{ "id": 12, "actorName": "管理员", "action": "item.take_down",
  "detail": "下架物品 a1b2...：信息不实", "ip": "192.168.1.100",
  "createdAt": "...", "createdAtText": "..." }
~~~

## 2. 元数据

GET /api/meta
~~~json
{ "ok": true, "data": {
  "categories": [ { "id": 1, "name": "证件卡类" } ],
  "locations": [ { "id": 2, "name": "图书馆", "building": "" } ],
  "types": [ { "id": 0, "name": "寻物" }, { "id": 1, "name": "招领" } ],
  "itemStatus": [ {"id":0,"name":"进行中"}, {"id":1,"name":"认领待审核"}, {"id":2,"name":"已通过待交接"},
                  {"id":3,"name":"已完成"}, {"id":4,"name":"已过期"}, {"id":5,"name":"已下架"} ],
  "claimStatus": [ {"id":0,"name":"待审核"}, {"id":1,"name":"已通过"}, {"id":2,"name":"已拒绝"},
                   {"id":3,"name":"已取消"}, {"id":4,"name":"已过期"}, {"id":5,"name":"已完成"} ] } }
~~~

## 3. 认证

POST /api/auth/register —— body {inviteCode, studentNo, name, college, password}
- 规则：邀请码有效、学号 4~20 位字母数字、姓名 2~20 字、密码 ≥6 位。
- 成功：{ "user": UserView }

POST /api/auth/login —— body {studentNo, password}
- 成功：{ "user": UserView } 并 Set-Cookie lf_sid；失败 401。
- 限流：同一 IP 10 次/分钟；同一账号连续失败 5 次锁定 15 分钟（返回 429）。

POST /api/auth/logout —— 清除 Cookie；返回 {}。

GET /api/auth/me —— { "user": UserView, "unreadCount": 2 }；未登录 401。

POST /api/auth/change-password —— { oldPassword, newPassword }

## 4. 物品

GET /api/items?type=&categoryId=&locationId=&status=active&q=&page=1&pageSize=12
- type：0 寻物 / 1 招领 / 空 全部；categoryId/locationId：0 或空 全部；
- status：active（默认，0/1/2）| open | claim_pending | claim_approved | completed | expired | removed | all；
- q：标题/描述关键词（中文子串匹配）；page 从 1 开始；pageSize 1~50。
- 返回：{ "items": [ItemView], "total": 37, "page": 1, "pageSize": 12 }

GET /api/items/{publicId} —— { "item": ItemView }

POST /api/items —— body {type, title, description, categoryId, locationId, eventTime, verifyQuestion, expiresInDays}
- title 2~60 字；description 1~2000 字；expiresInDays 1~90（默认 30）。
- 返回 { "item": ItemView }。

PATCH /api/items/{publicId} —— body {title, description, categoryId, locationId, eventTime, verifyQuestion}
- 仅发布者；仅 status=0 时可改。

DELETE /api/items/{publicId} —— 发布者或管理员；软删除（status=5）；返回 { "deleted": true }。

POST /api/items/{publicId}/images —— multipart/form-data，字段名 images（可多个，最多 3 个，单文件 ≤5MB，仅 jpg/png/webp）。
- 返回 { "images": [ { "id": "img1", "url": "/media/xxx.jpg" } ] }。

DELETE /api/items/{publicId}/images/{imageId} —— 发布者；返回 {}。

GET /media/{fileName} —— 直接返回图片（无鉴权，文件名随机不可枚举）。

## 5. 认领

POST /api/items/{publicId}/claims —— body {answer, message}
- 物品存在且 status=0；不能认领自己的帖子；有 verifyQuestion 时 answer 必填。
- 返回 { "claim": ClaimView }；同一物品已有进行中的认领时返回 409 CONFLICT。

GET /api/items/{publicId}/claims —— 发布者或管理员；返回 { "claims": [ClaimView] }。

GET /api/me/items?page=&pageSize= —— 我发布的物品；返回 { "items": [ItemView], "total": n }。

GET /api/me/claims —— 我提交的认领；返回 { "claims": [ClaimView] }。

POST /api/claims/{id}/approve —— 发布者或管理员；status 必须为 0。
- 生成 6 位取件码、24 小时有效；其他待审核申请自动拒绝。
- 返回 { "claim": ClaimView, "pickupCode": "123456" }。

POST /api/claims/{id}/reject —— body {reason}（可空）；发布者或管理员。
POST /api/claims/{id}/cancel —— 认领人本人取消。
POST /api/claims/{id}/verify —— body {code}；发布者或管理员核销；正确则认领完成、物品完成。
- 返回 { "claim": ClaimView }。

## 6. 通知

GET /api/notifications?unreadOnly=0&limit=50 —— { "notifications": [NotificationView], "unreadCount": 3 }
POST /api/notifications/read —— body {ids:[1,2], all:false}；返回 { "unreadCount": 0 }

## 7. 举报（普通用户）

POST /api/reports —— body {targetPublicId, reason}；返回 { "reported": true }

## 8. 管理后台（role=admin）

GET /api/admin/stats —— { "stats": {users,items,openItems,pendingClaims,completedClaims,openReports}, "recentAudit": [AuditView] }

GET /api/admin/reports —— { "reports": [ReportView] }
POST /api/admin/reports/{id}/handle —— body {action:"resolve"|"dismiss"}；返回 { "report": ReportView }

GET /api/admin/invites —— { "invites": [InviteView] }
POST /api/admin/invites —— body {maxUses, validDays}；返回 { "invite": InviteView+code }
- **完整邀请码 code 只在本次响应返回一次**，请立即保存；后续列表只有 codeMasked。

GET /api/admin/audit?limit=100 —— { "logs": [AuditView] }

POST /api/admin/items/{publicId}/take-down —— body {reason}；下架物品（status=5）并通知发布者。

## 9. 实时推送（SSE）

GET /api/stream —— text/event-stream（需登录，浏览器 EventSource 自动带 Cookie）。
每条消息格式：
~~~
event: message
data: {"type":"...","...":...}
~~~
心跳为注释行：": ping"（每 20 秒）。

事件类型：
- item-created：{ type, publicId, itemType, title } —— 广播
- item-updated：{ type, publicId, status } —— 广播
- claim-created：{ type, claimId, itemPublicId, itemTitle, claimantName } —— 仅物品发布者
- claim-updated：{ type, claimId, itemPublicId, status } —— 发给认领人与发布者
- notification：{ type, notification: NotificationView, unreadCount } —— 仅接收人

> 安全语义：注销、修改密码、会话空闲过期后，服务端会主动关闭对应 SSE 连接；
> 客户端收到中断后应重新请求 /api/auth/me，401 则回到登录页。

## 10. 首次启动与凭据（安全设计）

- 管理员：学号 **admin**；密码在**首次启动时随机生成并只在控制台打印一次**，不落库、不写源码、不进日志；登录后请立即修改。
- 邀请码：**不预置**。管理员登录后在「管理后台 → 邀请码」创建，完整码只在创建响应里返回一次；之后列表只有掩码，数据库只存 SHA-256 哈希。
- 普通用户通过管理员发放的邀请码注册；注册页/登录页不展示任何明文邀请码。
- 忘记管理员密码：删除 data 目录重启（数据清空，会生成新的随机密码）。

## 11. 状态文本

物品：0 进行中 / 1 认领待审核 / 2 已通过待交接 / 3 已完成 / 4 已过期 / 5 已下架
认领：0 待审核 / 1 已通过 / 2 已拒绝 / 3 已取消 / 4 已过期 / 5 已完成
