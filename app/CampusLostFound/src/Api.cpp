#include "Api.h"

#include "Auth.h"
#include "Http.h"
#include "Json.h"
#include "Store.h"
#include "Util.h"
#include "WebUi.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lf {

namespace {

const char* kSessionCookie = "lf_sid";
const int kSessionMaxAge = 12 * 60 * 60;

// ============================ 通用响应 ============================

JsonValue EmptyObject() {
    return JsonValue::MakeObject();
}

HttpResponse MakeOk(JsonValue data) {
    JsonValue root = JsonValue::MakeObject();
    root.Set("ok", JsonValue(true));
    root.Set("data", std::move(data));
    return HttpResponse::Json(root.Dump());
}

HttpResponse MakeError(const std::string& code, const std::string& message, int status) {
    JsonValue error = JsonValue::MakeObject();
    error.Set("code", JsonValue(code));
    error.Set("message", JsonValue(message));
    JsonValue root = JsonValue::MakeObject();
    root.Set("ok", JsonValue(false));
    root.Set("error", std::move(error));
    return HttpResponse::Json(root.Dump(), status);
}

std::string ItemTypeText(int type) {
    return type == 0 ? "寻物" : "招领";
}

std::string ItemStatusText(int status) {
    switch (status) {
        case 0: return "进行中";
        case 1: return "认领待审核";
        case 2: return "已通过待交接";
        case 3: return "已完成";
        case 4: return "已过期";
        case 5: return "已下架";
        default: return "未知状态";
    }
}

std::string ClaimStatusText(int status) {
    switch (status) {
        case 0: return "待审核";
        case 1: return "已通过";
        case 2: return "已拒绝";
        case 3: return "已取消";
        case 4: return "已过期";
        case 5: return "已完成";
        default: return "未知状态";
    }
}

// ============================ 会话 / 鉴权 ============================

std::optional<User> CurrentUser(const HttpRequest& req) {
    std::string token = req.CookieValue(kSessionCookie);
    if (token.empty()) return std::nullopt;
    auto userId = Auth::Instance().ValidateSession(token, true);
    if (!userId) return std::nullopt;
    auto user = Store::Instance().FindUserById(*userId);
    if (!user || user->status != 1) return std::nullopt;
    return user;
}

bool RequireUser(const HttpRequest& req, User& user, HttpResponse& error) {
    auto current = CurrentUser(req);
    if (!current) {
        error = MakeError("UNAUTHORIZED", "请先登录", 401);
        return false;
    }
    user = *current;
    return true;
}

bool RequireAdmin(const HttpRequest& req, User& user, HttpResponse& error) {
    if (!RequireUser(req, user, error)) return false;
    if (user.role != "admin") {
        error = MakeError("FORBIDDEN", "需要管理员权限", 403);
        return false;
    }
    return true;
}

bool CheckCsrf(const HttpRequest& req, HttpResponse& error) {
    if (req.method == "GET" || req.method == "HEAD" || req.method == "OPTIONS") return true;
    if (req.HeaderValue("x-requested-with") != "CampusLF") {
        error = MakeError("FORBIDDEN", "缺少 CSRF 校验请求头", 403);
        return false;
    }
    return true;
}

bool ReadJsonBody(const HttpRequest& req, JsonValue& body, HttpResponse& error) {
    if (req.body.empty()) {
        body = JsonValue::MakeObject();
        return true;
    }
    std::string parseError;
    if (!JsonValue::Parse(req.body, body, parseError)) {
        error = MakeError("INVALID_PARAM", "请求体不是合法 JSON", 400);
        return false;
    }
    if (!body.IsObject()) {
        error = MakeError("INVALID_PARAM", "请求体必须是 JSON 对象", 400);
        return false;
    }
    return true;
}

std::string StringField(const JsonValue& body, const std::string& key, const std::string& def = "") {
    const JsonValue* v = body.Find(key);
    return v ? v->AsString(def) : def;
}

int64_t IntField(const JsonValue& body, const std::string& key, int64_t def = 0) {
    const JsonValue* v = body.Find(key);
    return v ? v->AsInt(def) : def;
}

bool BoolField(const JsonValue& body, const std::string& key, bool def = false) {
    const JsonValue* v = body.Find(key);
    return v ? v->AsBool(def) : def;
}

// ============================ 视图 ============================

JsonValue MakeUserView(const User& user, bool fullName, bool fullStudentNo) {
    JsonValue v = JsonValue::MakeObject();
    v.Set("id", JsonValue(user.id));
    v.Set("studentNo", JsonValue(fullStudentNo ? user.studentNo : MaskStudentNo(user.studentNo)));
    v.Set("name", JsonValue(fullName ? user.name : MaskName(user.name)));
    v.Set("college", JsonValue(user.college));
    v.Set("role", JsonValue(user.role));
    v.Set("createdAt", JsonValue(FormatIso8601(user.createdAt)));
    return v;
}

JsonValue MakeItemView(const Item& item, const std::optional<User>& me, Store& store) {
    bool isOwner = me && me->id == item.userId;
    bool isAdmin = me && me->role == "admin";
    bool canManage = isOwner || isAdmin;

    bool hasActiveClaim = false;
    int myClaimStatus = -1;
    std::vector<Claim> claims = store.ClaimsForItem(item.id);
    if (me) {
        for (const Claim& claim : claims) {
            if (claim.claimantId == me->id) {
                myClaimStatus = claim.status;
                if (claim.status == 0 || claim.status == 1) hasActiveClaim = true;
            }
        }
    }
    bool claimAllowed = me && !isOwner && item.status == 0 && !hasActiveClaim;

    std::string categoryName;
    std::string locationName;
    std::string locationBuilding;
    for (const Category& c : store.Categories()) {
        if (c.id == item.categoryId) categoryName = c.name;
    }
    for (const Location& l : store.Locations()) {
        if (l.id == item.locationId) {
            locationName = l.name;
            locationBuilding = l.building;
        }
    }

    JsonValue images = JsonValue::MakeArray();
    for (const ItemImage& image : item.images) {
        JsonValue img = JsonValue::MakeObject();
        img.Set("id", JsonValue(image.id));
        img.Set("url", JsonValue("/media/" + image.fileName));
        images.Push(std::move(img));
    }

    JsonValue owner = JsonValue::MakeObject();
    auto ownerUser = store.FindUserById(item.userId);
    owner.Set("name", JsonValue(ownerUser ? MaskName(ownerUser->name) : std::string("匿名")));
    owner.Set("college", JsonValue(ownerUser ? ownerUser->college : std::string("")));

    JsonValue v = JsonValue::MakeObject();
    v.Set("publicId", JsonValue(item.publicId));
    v.Set("type", JsonValue(item.type));
    v.Set("typeText", JsonValue(ItemTypeText(item.type)));
    v.Set("title", JsonValue(item.title));
    v.Set("description", JsonValue(item.description));
    v.Set("categoryId", JsonValue(item.categoryId));
    v.Set("categoryName", JsonValue(categoryName));
    v.Set("locationId", JsonValue(item.locationId));
    v.Set("locationName", JsonValue(locationName));
    v.Set("locationBuilding", JsonValue(locationBuilding));
    v.Set("eventTime", JsonValue(item.eventTime));
    v.Set("status", JsonValue(item.status));
    v.Set("statusText", JsonValue(ItemStatusText(item.status)));
    v.Set("viewCount", JsonValue(item.viewCount));
    v.Set("createdAt", JsonValue(FormatIso8601(item.createdAt)));
    v.Set("createdAtText", JsonValue(FormatTimeAgo(item.createdAt)));
    v.Set("expiresAt", JsonValue(FormatIso8601(item.expiresAt)));
    v.Set("owner", std::move(owner));
    v.Set("images", std::move(images));
    v.Set("verifyQuestion", JsonValue(item.verifyQuestion));
    v.Set("claimCount", JsonValue(static_cast<int64_t>(claims.size())));
    v.Set("canManage", JsonValue(canManage));
    v.Set("claimAllowed", JsonValue(claimAllowed));
    v.Set("myClaimStatus", JsonValue(myClaimStatus));
    return v;
}

JsonValue MakeClaimView(const Claim& claim, const Item& item, Store& store, const std::optional<User>& me) {
    bool isOwner = me && me->id == item.userId;
    bool isAdmin = me && me->role == "admin";
    bool isClaimant = me && me->id == claim.claimantId;
    bool canReview = (isOwner || isAdmin) && claim.status == 0;
    bool canCancel = isClaimant && claim.status == 0;
    bool canVerify = (isOwner || isAdmin) && claim.status == 1;

    auto claimant = store.FindUserById(claim.claimantId);
    std::string claimantName = claimant ? MaskName(claimant->name) : std::string("匿名");
    if ((isOwner || isAdmin) && claimant) claimantName = claimant->name;

    JsonValue v = JsonValue::MakeObject();
    v.Set("id", JsonValue(claim.id));
    v.Set("itemPublicId", JsonValue(item.publicId));
    v.Set("itemTitle", JsonValue(item.title));
    v.Set("itemType", JsonValue(item.type));
    v.Set("claimantName", JsonValue(claimantName));
    v.Set("status", JsonValue(claim.status));
    v.Set("statusText", JsonValue(ClaimStatusText(claim.status)));
    v.Set("answer", JsonValue(claim.answer));
    v.Set("message", JsonValue(claim.message));
    v.Set("createdAt", JsonValue(FormatIso8601(claim.createdAt)));
    v.Set("createdAtText", JsonValue(FormatTimeAgo(claim.createdAt)));
    v.Set("reviewedAt", JsonValue(FormatIso8601(claim.reviewedAt)));
    v.Set("completedAt", JsonValue(FormatIso8601(claim.completedAt)));
    v.Set("pickupExpiresAt", JsonValue(FormatIso8601(claim.pickupExpiresAt)));
    v.Set("pickupCode", JsonValue(isClaimant ? claim.pickupCode : std::string()));
    v.Set("canReview", JsonValue(canReview));
    v.Set("canCancel", JsonValue(canCancel));
    v.Set("canVerify", JsonValue(canVerify));
    return v;
}

JsonValue MakeNotificationView(const Notification& n) {
    JsonValue v = JsonValue::MakeObject();
    v.Set("id", JsonValue(n.id));
    v.Set("type", JsonValue(n.type));
    v.Set("title", JsonValue(n.title));
    v.Set("body", JsonValue(n.body));
    v.Set("link", JsonValue(n.link));
    v.Set("read", JsonValue(n.read));
    v.Set("createdAt", JsonValue(FormatIso8601(n.createdAt)));
    v.Set("createdAtText", JsonValue(FormatTimeAgo(n.createdAt)));
    return v;
}

JsonValue MakeReportView(const Report& report, Store& store) {
    JsonValue v = JsonValue::MakeObject();
    v.Set("id", JsonValue(report.id));
    v.Set("targetType", JsonValue(report.targetType));
    std::string itemPublicId;
    std::string itemTitle;
    auto item = store.FindItemById(report.targetItemId);
    if (item) {
        itemPublicId = item->publicId;
        itemTitle = item->title;
    }
    v.Set("itemPublicId", JsonValue(itemPublicId));
    v.Set("itemTitle", JsonValue(itemTitle));
    v.Set("reason", JsonValue(report.reason));
    v.Set("status", JsonValue(report.status));
    std::string statusText = report.status == 0 ? "待处理" : (report.status == 1 ? "已处理" : "已驳回");
    v.Set("statusText", JsonValue(statusText));
    auto reporter = store.FindUserById(report.reporterId);
    v.Set("reporterName", JsonValue(reporter ? MaskName(reporter->name) : std::string("匿名")));
    v.Set("createdAt", JsonValue(FormatIso8601(report.createdAt)));
    v.Set("createdAtText", JsonValue(FormatTimeAgo(report.createdAt)));
    return v;
}

JsonValue MakeInviteView(const Invite& invite) {
    JsonValue v = JsonValue::MakeObject();
    v.Set("code", JsonValue(invite.code));
    v.Set("maxUses", JsonValue(invite.maxUses));
    v.Set("uses", JsonValue(invite.uses));
    v.Set("expiresAt", JsonValue(FormatIso8601(invite.expiresAt)));
    v.Set("expiresAtText", JsonValue(FormatLocalTime(invite.expiresAt)));
    v.Set("disabled", JsonValue(invite.disabled));
    v.Set("createdBy", JsonValue(invite.createdBy));
    v.Set("createdAt", JsonValue(FormatIso8601(invite.createdAt)));
    return v;
}

JsonValue MakeAuditView(const AuditLog& log, Store& store) {
    JsonValue v = JsonValue::MakeObject();
    v.Set("id", JsonValue(log.id));
    auto actor = store.FindUserById(log.actorId);
    v.Set("actorName", JsonValue(actor ? actor->name : std::string("系统")));
    v.Set("action", JsonValue(log.action));
    v.Set("detail", JsonValue(log.detail));
    v.Set("ip", JsonValue(log.ip));
    v.Set("createdAt", JsonValue(FormatIso8601(log.createdAt)));
    v.Set("createdAtText", JsonValue(FormatTimeAgo(log.createdAt)));
    return v;
}

// ============================ SSE 实时推送 ============================

struct SseClient {
    intptr_t socket = 0;
    int64_t userId = 0;
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::string> queue;
    bool closed = false;
};

std::mutex gSseMutex;
std::vector<std::shared_ptr<SseClient>> gSseClients;

void PushEvent(const std::string& eventJson, int64_t targetUserId) {
    std::string frame = "event: message\ndata: " + eventJson + "\n\n";
    std::vector<std::shared_ptr<SseClient>> targets;
    {
        std::lock_guard<std::mutex> lock(gSseMutex);
        for (const auto& client : gSseClients) {
            if (targetUserId == 0 || client->userId == targetUserId) targets.push_back(client);
        }
    }
    for (const auto& client : targets) {
        std::lock_guard<std::mutex> lock(client->mutex);
        if (client->closed) continue;
        client->queue.push_back(frame);
        client->cv.notify_one();
    }
}

void SendRawHttp(intptr_t socketHandle, int status, const std::string& body) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " +
                      (status == 401 ? "Unauthorized" : "Error") + "\r\n";
    out += "Content-Type: application/json; charset=utf-8\r\n";
    out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += body;
    SocketSendAll(socketHandle, out);
}

void HandleStream(const HttpRequest& req, intptr_t socketHandle) {
    auto current = CurrentUser(req);
    if (!current) {
        std::string body =
            "{\"ok\":false,\"error\":{\"code\":\"UNAUTHORIZED\",\"message\":\"请先登录\"}}";
        SendRawHttp(socketHandle, 401, body);
        return;
    }

    std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "X-Accel-Buffering: no\r\n\r\n";
    if (!SocketSendAll(socketHandle, headers)) return;

    auto client = std::make_shared<SseClient>();
    client->socket = socketHandle;
    client->userId = current->id;
    {
        std::lock_guard<std::mutex> lock(gSseMutex);
        gSseClients.push_back(client);
    }

    while (true) {
        std::deque<std::string> frames;
        {
            std::unique_lock<std::mutex> lock(client->mutex);
            client->cv.wait_for(lock, std::chrono::seconds(20), [&client]() {
                return !client->queue.empty() || client->closed;
            });
            frames.swap(client->queue);
        }
        if (frames.empty()) {
            if (!SocketSendAll(socketHandle, ": ping\n\n")) break;
        } else {
            bool ok = true;
            for (const std::string& frame : frames) {
                if (!SocketSendAll(socketHandle, frame)) {
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(client->mutex);
        client->closed = true;
    }
    {
        std::lock_guard<std::mutex> lock(gSseMutex);
        gSseClients.erase(
            std::remove_if(gSseClients.begin(), gSseClients.end(),
                           [&client](const std::shared_ptr<SseClient>& c) { return c.get() == client.get(); }),
            gSseClients.end());
    }
}

// ============================ 上传文件辅助 ============================

bool IsJpeg(const std::string& data) {
    return data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xFF &&
           static_cast<unsigned char>(data[1]) == 0xD8 && static_cast<unsigned char>(data[2]) == 0xFF;
}

bool IsPng(const std::string& data) {
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return data.size() >= 8 && std::memcmp(data.data(), sig, 8) == 0;
}

bool IsWebp(const std::string& data) {
    return data.size() >= 12 && data.compare(0, 4, "RIFF") == 0 && data.compare(8, 4, "WEBP") == 0;
}

std::string ImageExtension(const std::string& data) {
    if (IsJpeg(data)) return ".jpg";
    if (IsPng(data)) return ".png";
    if (IsWebp(data)) return ".webp";
    return std::string();
}

std::string ImageContentType(const std::string& fileName) {
    if (EndsWith(ToLowerAscii(fileName), ".png")) return "image/png";
    if (EndsWith(ToLowerAscii(fileName), ".webp")) return "image/webp";
    return "image/jpeg";
}

std::string UploadsDir() {
    return Store::Instance().DataDir() + "/uploads";
}

bool SaveUploadFile(const std::string& fileName, const std::string& data) {
    std::error_code ec;
    std::filesystem::create_directories(UploadsDir(), ec);
    std::ofstream out(UploadsDir() + "/" + fileName, std::ios::binary);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();
    return static_cast<bool>(out);
}

bool RemoveUploadFile(const std::string& fileName) {
    std::error_code ec;
    return std::filesystem::remove(UploadsDir() + "/" + fileName, ec);
}

// ============================ 参数校验 ============================

bool ValidItemFields(const std::string& title, const std::string& description, int type,
                     int categoryId, int locationId, const std::string& eventTime,
                     const std::string& verifyQuestion, std::string& error) {
    if (type != 0 && type != 1) {
        error = "物品类型不正确";
        return false;
    }
    if (title.empty() || title.size() > 200) {
        error = "标题不能为空且不超过 60 个汉字";
        return false;
    }
    if (description.empty() || description.size() > 6000) {
        error = "描述不能为空且不超过 2000 字";
        return false;
    }
    if (eventTime.size() > 120) {
        error = "时间描述过长";
        return false;
    }
    if (verifyQuestion.size() > 300) {
        error = "核验问题不超过 100 个汉字";
        return false;
    }
    bool categoryOk = false;
    for (const Category& c : Store::Instance().Categories()) {
        if (c.id == categoryId) categoryOk = true;
    }
    if (!categoryOk) {
        error = "请选择正确的物品分类";
        return false;
    }
    bool locationOk = false;
    for (const Location& l : Store::Instance().Locations()) {
        if (l.id == locationId) locationOk = true;
    }
    if (!locationOk) {
        error = "请选择正确的地点";
        return false;
    }
    return true;
}

// ============================ 路由处理 ============================

HttpResponse HandleRegister(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;

    std::string inviteCode = Trim(StringField(body, "inviteCode"));
    std::string studentNo = Trim(StringField(body, "studentNo"));
    std::string name = Trim(StringField(body, "name"));
    std::string college = Trim(StringField(body, "college"));
    std::string password = StringField(body, "password");

    if (inviteCode.empty()) return MakeError("INVALID_PARAM", "请填写邀请码", 400);
    if (studentNo.size() < 4 || studentNo.size() > 32) return MakeError("INVALID_PARAM", "学号长度不正确", 400);
    for (char c : studentNo) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!ok) return MakeError("INVALID_PARAM", "学号只能包含字母和数字", 400);
    }
    if (name.empty() || name.size() > 60) return MakeError("INVALID_PARAM", "姓名长度不正确", 400);
    if (college.empty() || college.size() > 120) return MakeError("INVALID_PARAM", "请填写学院", 400);
    if (password.size() < 6 || password.size() > 64) return MakeError("INVALID_PARAM", "密码长度需为 6~64 位", 400);
    if (Store::Instance().FindUserByStudentNo(studentNo)) {
        return MakeError("CONFLICT", "该学号已注册", 409);
    }
    if (!Auth::Instance().AllowAttempt("reg:ip:" + req.remoteIp, 10, 3600)) {
        return MakeError("RATE_LIMITED", "注册过于频繁，请稍后再试", 429);
    }
    auto invite = Store::Instance().FindInvite(inviteCode);
    if (!invite || invite->disabled || invite->uses >= invite->maxUses ||
        (invite->expiresAt > 0 && invite->expiresAt < NowEpoch())) {
        return MakeError("INVALID_PARAM", "邀请码无效或已用完", 400);
    }
    std::string passwordHash = Auth::Instance().HashPassword(password);
    if (passwordHash.empty()) return MakeError("INTERNAL", "密码处理失败", 500);
    std::string storeError;
    int64_t userId = Store::Instance().CreateUser(studentNo, name, college, passwordHash, "student", storeError);
    if (userId <= 0) return MakeError("INTERNAL", storeError.empty() ? "注册失败" : storeError, 500);
    if (!Store::Instance().ConsumeInvite(inviteCode, storeError)) {
        return MakeError("INTERNAL", "邀请码核销失败", 500);
    }
    Store::Instance().AddAudit(userId, "auth.register", "新用户注册：" + studentNo, req.remoteIp);

    std::string token = Auth::Instance().CreateSession(userId);
    auto user = Store::Instance().FindUserById(userId);
    HttpResponse response = MakeOk(MakeUserView(*user, true, true));
    response.AddCookie(kSessionCookie, token, kSessionMaxAge, true, false);
    return response;
}

HttpResponse HandleLogin(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string studentNo = Trim(StringField(body, "studentNo"));
    std::string password = StringField(body, "password");
    if (studentNo.empty() || password.empty()) return MakeError("INVALID_PARAM", "请填写学号和密码", 400);

    if (!Auth::Instance().AllowAttempt("login:ip:" + req.remoteIp, 30, 60)) {
        return MakeError("RATE_LIMITED", "登录尝试过于频繁，请稍后再试", 429);
    }
    if (!Auth::Instance().AllowAttempt("login:user:" + studentNo, 5, 900)) {
        return MakeError("RATE_LIMITED", "该账号尝试次数过多，请 15 分钟后再试", 429);
    }
    auto user = Store::Instance().FindUserByStudentNo(studentNo);
    if (!user || user->status != 1 || !Auth::Instance().VerifyPassword(password, user->passwordHash)) {
        return MakeError("UNAUTHORIZED", "学号或密码错误", 401);
    }
    Auth::Instance().ResetAttempts("login:user:" + studentNo);
    Store::Instance().UpdateLastLogin(user->id);
    Store::Instance().AddAudit(user->id, "auth.login", "登录成功", req.remoteIp);

    std::string token = Auth::Instance().CreateSession(user->id);
    HttpResponse response = MakeOk(MakeUserView(*user, true, true));
    response.AddCookie(kSessionCookie, token, kSessionMaxAge, true, false);
    return response;
}

HttpResponse HandleLogout(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    std::string token = req.CookieValue(kSessionCookie);
    if (!token.empty()) {
        auto userId = Auth::Instance().ValidateSession(token, false);
        Auth::Instance().RevokeSession(token);
        if (userId) Store::Instance().AddAudit(*userId, "auth.logout", "退出登录", req.remoteIp);
    }
    HttpResponse response = MakeOk(EmptyObject());
    response.ClearCookie(kSessionCookie);
    return response;
}

HttpResponse HandleMe(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    auto self = Store::Instance().FindUserById(user.id);
    JsonValue data = JsonValue::MakeObject();
    data.Set("user", MakeUserView(*self, true, true));
    data.Set("unreadCount", JsonValue(Store::Instance().UnreadCount(user.id)));
    return MakeOk(std::move(data));
}

HttpResponse HandleChangePassword(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string oldPassword = StringField(body, "oldPassword");
    std::string newPassword = StringField(body, "newPassword");
    if (newPassword.size() < 6 || newPassword.size() > 64) {
        return MakeError("INVALID_PARAM", "新密码长度需为 6~64 位", 400);
    }
    auto stored = Store::Instance().FindUserById(user.id);
    if (!stored || !Auth::Instance().VerifyPassword(oldPassword, stored->passwordHash)) {
        return MakeError("INVALID_PARAM", "原密码不正确", 400);
    }
    std::string newHash = Auth::Instance().HashPassword(newPassword);
    if (newHash.empty() || !Store::Instance().ChangePassword(user.id, newHash)) {
        return MakeError("INTERNAL", "修改密码失败", 500);
    }
    Auth::Instance().RevokeAllForUser(user.id);
    std::string token = Auth::Instance().CreateSession(user.id);
    Store::Instance().AddAudit(user.id, "auth.change_password", "修改密码", req.remoteIp);
    HttpResponse response = MakeOk(EmptyObject());
    response.AddCookie(kSessionCookie, token, kSessionMaxAge, true, false);
    return response;
}

HttpResponse HandleMeta(const HttpRequest& req) {
    (void)req;
    Store& store = Store::Instance();
    JsonValue categories = JsonValue::MakeArray();
    for (const Category& c : store.Categories()) {
        JsonValue v = JsonValue::MakeObject();
        v.Set("id", JsonValue(c.id));
        v.Set("name", JsonValue(c.name));
        categories.Push(std::move(v));
    }
    JsonValue locations = JsonValue::MakeArray();
    for (const Location& l : store.Locations()) {
        JsonValue v = JsonValue::MakeObject();
        v.Set("id", JsonValue(l.id));
        v.Set("name", JsonValue(l.name));
        v.Set("building", JsonValue(l.building));
        locations.Push(std::move(v));
    }
    JsonValue types = JsonValue::MakeArray();
    {
        JsonValue v = JsonValue::MakeObject();
        v.Set("id", JsonValue(0));
        v.Set("name", JsonValue("寻物"));
        types.Push(std::move(v));
        JsonValue v2 = JsonValue::MakeObject();
        v2.Set("id", JsonValue(1));
        v2.Set("name", JsonValue("招领"));
        types.Push(std::move(v2));
    }
    JsonValue itemStatus = JsonValue::MakeArray();
    for (int i = 0; i <= 5; ++i) {
        JsonValue v = JsonValue::MakeObject();
        v.Set("id", JsonValue(i));
        v.Set("name", JsonValue(ItemStatusText(i)));
        itemStatus.Push(std::move(v));
    }
    JsonValue claimStatus = JsonValue::MakeArray();
    for (int i = 0; i <= 5; ++i) {
        JsonValue v = JsonValue::MakeObject();
        v.Set("id", JsonValue(i));
        v.Set("name", JsonValue(ClaimStatusText(i)));
        claimStatus.Push(std::move(v));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("categories", std::move(categories));
    data.Set("locations", std::move(locations));
    data.Set("types", std::move(types));
    data.Set("itemStatus", std::move(itemStatus));
    data.Set("claimStatus", std::move(claimStatus));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemList(const HttpRequest& req) {
    Store& store = Store::Instance();
    auto me = CurrentUser(req);
    int type = -1;
    std::string typeStr = req.QueryValue("type");
    if (typeStr == "0" || typeStr == "1") type = typeStr[0] - '0';
    int categoryId = 0;
    int locationId = 0;
    ParseInt(req.QueryValue("categoryId", "0"), categoryId);
    ParseInt(req.QueryValue("locationId", "0"), locationId);
    std::string status = req.QueryValue("status", "active");
    std::string keyword = Trim(req.QueryValue("q"));
    int page = 1;
    int pageSize = 12;
    ParseInt(req.QueryValue("page", "1"), page);
    ParseInt(req.QueryValue("pageSize", "12"), pageSize);
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 1;
    if (pageSize > 50) pageSize = 50;

    int total = 0;
    std::vector<Item> items = store.QueryItems(type, categoryId, locationId, status, keyword,
                                               (page - 1) * pageSize, pageSize, total);
    JsonValue array = JsonValue::MakeArray();
    for (const Item& item : items) {
        array.Push(MakeItemView(item, me, store));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("items", std::move(array));
    data.Set("total", JsonValue(total));
    data.Set("page", JsonValue(page));
    data.Set("pageSize", JsonValue(pageSize));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemCreate(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;

    int type = static_cast<int>(IntField(body, "type", 1));
    std::string title = Trim(StringField(body, "title"));
    std::string description = Trim(StringField(body, "description"));
    int categoryId = static_cast<int>(IntField(body, "categoryId"));
    int locationId = static_cast<int>(IntField(body, "locationId"));
    std::string eventTime = Trim(StringField(body, "eventTime"));
    std::string verifyQuestion = Trim(StringField(body, "verifyQuestion"));
    int expiresInDays = static_cast<int>(IntField(body, "expiresInDays", 30));
    if (expiresInDays < 1) expiresInDays = 1;
    if (expiresInDays > 90) expiresInDays = 90;

    std::string validationError;
    if (!ValidItemFields(title, description, type, categoryId, locationId, eventTime, verifyQuestion, validationError)) {
        return MakeError("INVALID_PARAM", validationError, 400);
    }
    if (!Auth::Instance().AllowAttempt("post:" + std::to_string(user.id), 30, 3600)) {
        return MakeError("RATE_LIMITED", "发布过于频繁，请稍后再试", 429);
    }

    Item item;
    item.type = type;
    item.userId = user.id;
    item.title = title;
    item.description = description;
    item.categoryId = categoryId;
    item.locationId = locationId;
    item.eventTime = eventTime;
    item.verifyQuestion = verifyQuestion;
    item.expiresAt = NowEpoch() + static_cast<int64_t>(expiresInDays) * 86400;
    std::string storeError;
    if (!Store::Instance().CreateItem(item, storeError)) {
        return MakeError("INVALID_PARAM", storeError.empty() ? "发布失败" : storeError, 400);
    }
    Store::Instance().AddAudit(user.id, "item.create", "发布物品：" + item.title, req.remoteIp);
    auto fresh = Store::Instance().FindItemById(item.id);
    JsonValue data = JsonValue::MakeObject();
    data.Set("item", MakeItemView(*fresh, user, Store::Instance()));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemDetail(const HttpRequest& req) {
    Store& store = Store::Instance();
    auto me = CurrentUser(req);
    std::string publicId = req.PathValue("publicId");
    auto item = store.FindItemByPublicId(publicId);
    if (!item) return MakeError("NOT_FOUND", "物品不存在或已删除", 404);
    bool isOwner = me && me->id == item->userId;
    bool isAdmin = me && me->role == "admin";
    if (item->status == 5 && !isOwner && !isAdmin) {
        return MakeError("NOT_FOUND", "物品不存在或已删除", 404);
    }
    store.IncrementView(item->id);
    item->viewCount += 1;
    JsonValue data = JsonValue::MakeObject();
    data.Set("item", MakeItemView(*item, me, store));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemUpdate(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId != user.id && user.role != "admin") return MakeError("FORBIDDEN", "无权修改该物品", 403);
    if (item->status != 0) return MakeError("CONFLICT", "当前状态不可修改", 409);
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string title = Trim(StringField(body, "title"));
    std::string description = Trim(StringField(body, "description"));
    int categoryId = static_cast<int>(IntField(body, "categoryId"));
    int locationId = static_cast<int>(IntField(body, "locationId"));
    std::string eventTime = Trim(StringField(body, "eventTime"));
    std::string verifyQuestion = Trim(StringField(body, "verifyQuestion"));
    std::string validationError;
    if (!ValidItemFields(title, description, item->type, categoryId, locationId, eventTime, verifyQuestion, validationError)) {
        return MakeError("INVALID_PARAM", validationError, 400);
    }
    std::string storeError;
    if (!store.UpdateItem(item->id, title, description, categoryId, locationId, eventTime, verifyQuestion, storeError)) {
        return MakeError("INVALID_PARAM", storeError.empty() ? "修改失败" : storeError, 400);
    }
    auto fresh = store.FindItemById(item->id);
    JsonValue data = JsonValue::MakeObject();
    data.Set("item", MakeItemView(*fresh, user, store));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemDelete(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId != user.id && user.role != "admin") return MakeError("FORBIDDEN", "无权删除该物品", 403);
    std::string storeError;
    if (!store.SetItemStatus(item->id, 5, user.role == "admin" ? "管理员删除" : "发布者删除", storeError)) {
        return MakeError("CONFLICT", storeError.empty() ? "删除失败" : storeError, 409);
    }
    if (user.role == "admin") {
        store.AddAudit(user.id, "item.delete", "删除物品：" + item->title, req.remoteIp);
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("deleted", JsonValue(true));
    return MakeOk(std::move(data));
}

HttpResponse HandleImageUpload(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId != user.id && user.role != "admin") return MakeError("FORBIDDEN", "无权上传图片", 403);
    if (item->status != 0) return MakeError("CONFLICT", "当前状态不可上传图片", 409);
    if (req.uploads.empty()) return MakeError("INVALID_PARAM", "没有收到图片文件", 400);
    if (item->images.size() + req.uploads.size() > 3) {
        return MakeError("INVALID_PARAM", "每个物品最多 3 张图片", 400);
    }
    if (!Auth::Instance().AllowAttempt("upload:" + std::to_string(user.id), 60, 3600)) {
        return MakeError("RATE_LIMITED", "上传过于频繁，请稍后再试", 429);
    }
    JsonValue images = JsonValue::MakeArray();
    for (const HttpRequest::Upload& upload : req.uploads) {
        if (upload.data.size() > 5 * 1024 * 1024) {
            return MakeError("INVALID_PARAM", "单张图片不能超过 5MB", 400);
        }
        std::string ext = ImageExtension(upload.data);
        if (ext.empty()) return MakeError("INVALID_PARAM", "仅支持 JPG/PNG/WebP 图片", 400);
        std::string fileName = RandomHex(16) + ext;
        if (!SaveUploadFile(fileName, upload.data)) {
            return MakeError("INTERNAL", "保存图片失败", 500);
        }
        std::string imageId;
        std::string storeError;
        if (!store.AddItemImage(item->id, fileName, imageId, storeError)) {
            RemoveUploadFile(fileName);
            return MakeError("INVALID_PARAM", storeError.empty() ? "添加图片失败" : storeError, 400);
        }
        JsonValue img = JsonValue::MakeObject();
        img.Set("id", JsonValue(imageId));
        img.Set("url", JsonValue("/media/" + fileName));
        images.Push(std::move(img));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("images", std::move(images));
    return MakeOk(std::move(data));
}

HttpResponse HandleImageDelete(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId != user.id && user.role != "admin") return MakeError("FORBIDDEN", "无权删除图片", 403);
    std::string imageId = req.PathValue("imageId");
    std::string removedFileName;
    for (const ItemImage& image : item->images) {
        if (image.id == imageId) removedFileName = image.fileName;
    }
    std::string storeError;
    if (!store.RemoveItemImage(item->id, imageId, storeError)) {
        return MakeError("NOT_FOUND", storeError.empty() ? "图片不存在" : storeError, 404);
    }
    if (!removedFileName.empty()) RemoveUploadFile(removedFileName);
    return MakeOk(EmptyObject());
}

HttpResponse HandleMedia(const HttpRequest& req) {
    std::string fileName = req.PathValue("fileName");
    std::string safeName = SanitizeAscii(fileName);
    if (safeName.empty() || safeName != fileName) return MakeError("NOT_FOUND", "图片不存在", 404);
    std::string path = UploadsDir() + "/" + safeName;
    std::ifstream in(path, std::ios::binary);
    if (!in) return MakeError("NOT_FOUND", "图片不存在", 404);
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    HttpResponse response = HttpResponse::Binary(data, ImageContentType(safeName));
    response.headers.push_back({"Cache-Control", "public, max-age=86400"});
    return response;
}

HttpResponse HandleClaimCreate(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId == user.id) return MakeError("INVALID_PARAM", "不能认领自己发布的物品", 400);
    if (item->status != 0) return MakeError("CONFLICT", "该物品当前不可认领", 409);
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string answer = Trim(StringField(body, "answer"));
    std::string message = Trim(StringField(body, "message"));
    if (!item->verifyQuestion.empty() && answer.empty()) {
        return MakeError("INVALID_PARAM", "请回答核验问题", 400);
    }
    if (answer.size() > 300 || message.size() > 500) {
        return MakeError("INVALID_PARAM", "回答或留言过长", 400);
    }
    Claim claim;
    claim.itemId = item->id;
    claim.claimantId = user.id;
    claim.answer = answer;
    claim.message = message;
    std::string storeError;
    if (!store.CreateClaim(claim, storeError)) {
        return MakeError("CONFLICT", storeError.empty() ? "认领失败" : storeError, 409);
    }
    auto fresh = store.FindClaimById(claim.id);
    auto freshItem = store.FindItemById(item->id);
    JsonValue data = JsonValue::MakeObject();
    data.Set("claim", MakeClaimView(*fresh, *freshItem, store, user));
    return MakeOk(std::move(data));
}

HttpResponse HandleItemClaims(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    if (item->userId != user.id && user.role != "admin") return MakeError("FORBIDDEN", "无权查看认领申请", 403);
    JsonValue array = JsonValue::MakeArray();
    for (const Claim& claim : store.ClaimsForItem(item->id)) {
        array.Push(MakeClaimView(claim, *item, store, user));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("claims", std::move(array));
    return MakeOk(std::move(data));
}

HttpResponse HandleMyItems(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    int page = 1;
    int pageSize = 50;
    ParseInt(req.QueryValue("page", "1"), page);
    ParseInt(req.QueryValue("pageSize", "50"), pageSize);
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 1;
    if (pageSize > 100) pageSize = 100;
    int total = 0;
    std::vector<Item> all = store.QueryItems(-1, 0, 0, "all", "", 0, 10000, total);
    std::vector<Item> mine;
    for (const Item& item : all) {
        if (item.userId == user.id) mine.push_back(item);
    }
    int start = (page - 1) * pageSize;
    JsonValue array = JsonValue::MakeArray();
    for (int i = start; i < static_cast<int>(mine.size()) && i < start + pageSize; ++i) {
        array.Push(MakeItemView(mine[static_cast<size_t>(i)], user, store));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("items", std::move(array));
    data.Set("total", JsonValue(static_cast<int64_t>(mine.size())));
    return MakeOk(std::move(data));
}

HttpResponse HandleMyClaims(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    JsonValue array = JsonValue::MakeArray();
    for (const Claim& claim : store.ClaimsByUser(user.id)) {
        auto item = store.FindItemById(claim.itemId);
        if (!item) continue;
        array.Push(MakeClaimView(claim, *item, store, user));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("claims", std::move(array));
    return MakeOk(std::move(data));
}

HttpResponse HandleClaimAction(const HttpRequest& req, const std::string& action) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    int64_t claimId = 0;
    if (!ParseInt64(req.PathValue("id"), claimId)) return MakeError("INVALID_PARAM", "认领编号不正确", 400);
    std::string storeError;
    Claim claim;
    if (action == "approve") {
        if (!store.ApproveClaim(claimId, user.id, claim, storeError)) {
            return MakeError("CONFLICT", storeError.empty() ? "操作失败" : storeError, 409);
        }
        auto item = store.FindItemById(claim.itemId);
        JsonValue data = JsonValue::MakeObject();
        data.Set("claim", MakeClaimView(claim, *item, store, user));
        data.Set("pickupCode", JsonValue(claim.pickupCode));
        if (user.role == "admin") {
            store.AddAudit(user.id, "claim.approve", "管理员通过认领申请 #" + std::to_string(claimId), req.remoteIp);
        }
        return MakeOk(std::move(data));
    }
    if (action == "reject") {
        if (!store.RejectClaim(claimId, user.id, claim, storeError)) {
            return MakeError("CONFLICT", storeError.empty() ? "操作失败" : storeError, 409);
        }
    } else if (action == "cancel") {
        if (!store.CancelClaim(claimId, user.id, claim, storeError)) {
            return MakeError("CONFLICT", storeError.empty() ? "操作失败" : storeError, 409);
        }
    } else if (action == "verify") {
        JsonValue body;
        if (!ReadJsonBody(req, body, error)) return error;
        std::string code = Trim(StringField(body, "code"));
        if (code.empty()) return MakeError("INVALID_PARAM", "请输入取件码", 400);
        if (!store.VerifyClaim(claimId, user.id, code, claim, storeError)) {
            return MakeError("CONFLICT", storeError.empty() ? "核销失败" : storeError, 409);
        }
    } else {
        return MakeError("NOT_FOUND", "接口不存在", 404);
    }
    auto item = store.FindItemById(claim.itemId);
    JsonValue data = JsonValue::MakeObject();
    data.Set("claim", MakeClaimView(claim, *item, store, user));
    return MakeOk(std::move(data));
}

HttpResponse HandleNotifications(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    Store& store = Store::Instance();
    bool unreadOnly = req.QueryValue("unreadOnly") == "1";
    int limit = 50;
    ParseInt(req.QueryValue("limit", "50"), limit);
    if (limit < 1) limit = 1;
    if (limit > 200) limit = 200;
    JsonValue array = JsonValue::MakeArray();
    for (const Notification& n : store.NotificationsFor(user.id, unreadOnly, limit)) {
        array.Push(MakeNotificationView(n));
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("notifications", std::move(array));
    data.Set("unreadCount", JsonValue(store.UnreadCount(user.id)));
    return MakeOk(std::move(data));
}

HttpResponse HandleNotificationsRead(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    bool all = BoolField(body, "all", false);
    std::vector<int64_t> ids;
    const JsonValue* idsValue = body.Find("ids");
    if (idsValue && idsValue->IsArray()) {
        for (const JsonValue& item : idsValue->Items()) {
            if (item.IsInt()) ids.push_back(item.AsInt());
        }
    }
    Store::Instance().MarkNotificationsRead(user.id, ids, all);
    JsonValue data = JsonValue::MakeObject();
    data.Set("unreadCount", JsonValue(Store::Instance().UnreadCount(user.id)));
    return MakeOk(std::move(data));
}

HttpResponse HandleReportCreate(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireUser(req, user, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string publicId = Trim(StringField(body, "targetPublicId"));
    std::string reason = Trim(StringField(body, "reason"));
    if (publicId.empty() || reason.empty() || reason.size() > 500) {
        return MakeError("INVALID_PARAM", "请填写举报原因", 400);
    }
    auto item = Store::Instance().FindItemByPublicId(publicId);
    if (!item) return MakeError("NOT_FOUND", "举报目标不存在", 404);
    if (!Auth::Instance().AllowAttempt("report:" + std::to_string(user.id), 20, 3600)) {
        return MakeError("RATE_LIMITED", "举报过于频繁", 429);
    }
    int64_t reportId = Store::Instance().AddReport(user.id, item->id, reason);
    Store::Instance().AddAudit(user.id, "report.create",
                               "举报物品 #" + std::to_string(reportId) + "：" + item->title, req.remoteIp);
    JsonValue data = JsonValue::MakeObject();
    data.Set("reported", JsonValue(true));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminStats(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    Store& store = Store::Instance();
    StoreStats stats = store.GetStats();
    JsonValue statsJson = JsonValue::MakeObject();
    statsJson.Set("users", JsonValue(stats.users));
    statsJson.Set("items", JsonValue(stats.items));
    statsJson.Set("openItems", JsonValue(stats.openItems));
    statsJson.Set("pendingClaims", JsonValue(stats.pendingClaims));
    statsJson.Set("completedClaims", JsonValue(stats.completedClaims));
    statsJson.Set("openReports", JsonValue(stats.openReports));
    JsonValue auditArray = JsonValue::MakeArray();
    for (const AuditLog& log : store.AuditLogs(10)) auditArray.Push(MakeAuditView(log, store));
    JsonValue data = JsonValue::MakeObject();
    data.Set("stats", std::move(statsJson));
    data.Set("recentAudit", std::move(auditArray));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminReports(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    Store& store = Store::Instance();
    JsonValue array = JsonValue::MakeArray();
    for (const Report& report : store.AllReports()) array.Push(MakeReportView(report, store));
    JsonValue data = JsonValue::MakeObject();
    data.Set("reports", std::move(array));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminReportHandle(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    int64_t reportId = 0;
    if (!ParseInt64(req.PathValue("id"), reportId)) return MakeError("INVALID_PARAM", "举报编号不正确", 400);
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string action = Trim(StringField(body, "action"));
    int status = action == "resolve" ? 1 : (action == "dismiss" ? 2 : 0);
    if (status == 0) return MakeError("INVALID_PARAM", "action 只能是 resolve 或 dismiss", 400);
    Store& store = Store::Instance();
    std::string storeError;
    if (!store.UpdateReportStatus(reportId, status, user.id, storeError)) {
        return MakeError("NOT_FOUND", storeError.empty() ? "举报不存在" : storeError, 404);
    }
    store.AddAudit(user.id, "report.handle", "处理举报 #" + std::to_string(reportId), req.remoteIp);
    Report report;
    for (const Report& r : store.AllReports()) {
        if (r.id == reportId) report = r;
    }
    JsonValue data = JsonValue::MakeObject();
    data.Set("report", MakeReportView(report, store));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminInvites(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    Store& store = Store::Instance();
    JsonValue array = JsonValue::MakeArray();
    for (const Invite& invite : store.AllInvites()) array.Push(MakeInviteView(invite));
    JsonValue data = JsonValue::MakeObject();
    data.Set("invites", std::move(array));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminInviteCreate(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    int maxUses = static_cast<int>(IntField(body, "maxUses", 50));
    int validDays = static_cast<int>(IntField(body, "validDays", 30));
    if (maxUses < 1 || maxUses > 10000) return MakeError("INVALID_PARAM", "可用次数 1~10000", 400);
    if (validDays < 1 || validDays > 3650) return MakeError("INVALID_PARAM", "有效期 1~3650 天", 400);
    Invite invite = Store::Instance().CreateInvite(maxUses, validDays, user.studentNo);
    Store::Instance().AddAudit(user.id, "invite.create", "创建邀请码：" + invite.code, req.remoteIp);
    JsonValue data = JsonValue::MakeObject();
    data.Set("invite", MakeInviteView(invite));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminAudit(const HttpRequest& req) {
    HttpResponse error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    int limit = 100;
    ParseInt(req.QueryValue("limit", "100"), limit);
    if (limit < 1) limit = 1;
    if (limit > 500) limit = 500;
    Store& store = Store::Instance();
    JsonValue array = JsonValue::MakeArray();
    for (const AuditLog& log : store.AuditLogs(limit)) array.Push(MakeAuditView(log, store));
    JsonValue data = JsonValue::MakeObject();
    data.Set("logs", std::move(array));
    return MakeOk(std::move(data));
}

HttpResponse HandleAdminTakeDown(const HttpRequest& req) {
    HttpResponse error;
    if (!CheckCsrf(req, error)) return error;
    User user;
    if (!RequireAdmin(req, user, error)) return error;
    Store& store = Store::Instance();
    auto item = store.FindItemByPublicId(req.PathValue("publicId"));
    if (!item) return MakeError("NOT_FOUND", "物品不存在", 404);
    JsonValue body;
    if (!ReadJsonBody(req, body, error)) return error;
    std::string reason = Trim(StringField(body, "reason"));
    if (reason.empty()) reason = "管理员下架";
    std::string storeError;
    if (!store.SetItemStatus(item->id, 5, reason, storeError)) {
        return MakeError("CONFLICT", storeError.empty() ? "下架失败" : storeError, 409);
    }
    store.AddAudit(user.id, "item.take_down", "下架物品：" + item->title + "（" + reason + "）", req.remoteIp);
    auto fresh = store.FindItemById(item->id);
    JsonValue data = JsonValue::MakeObject();
    data.Set("item", MakeItemView(*fresh, user, store));
    return MakeOk(std::move(data));
}

HttpResponse HandleHealth(const HttpRequest& req) {
    (void)req;
    JsonValue data = JsonValue::MakeObject();
    data.Set("status", JsonValue("ok"));
    data.Set("time", JsonValue(FormatIso8601(NowEpoch())));
    return MakeOk(std::move(data));
}

void BackgroundLoop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        Store::Instance().ExpireOverdue();
        Auth::Instance().SweepExpired();
    }
}

} // namespace

void Api::RegisterRoutes(HttpServer& server) {
    Store::Instance().SetEventSink([](const std::string& eventJson, int64_t targetUserId) {
        PushEvent(eventJson, targetUserId);
    });

    // 静态页面（单页应用）
    server.Route("GET", "/", [](const HttpRequest&) { return HttpResponse::Html(WebUiHtml()); });
    server.Route("GET", "/index.html", [](const HttpRequest&) { return HttpResponse::Html(WebUiHtml()); });
    server.Route("GET", "/favicon.ico", [](const HttpRequest&) {
        HttpResponse r;
        r.status = 204;
        r.contentType = "image/x-icon";
        return r;
    });

    // 健康检查 / 元数据
    server.Route("GET", "/api/health", HandleHealth);
    server.Route("GET", "/api/meta", HandleMeta);

    // 认证
    server.Route("POST", "/api/auth/register", HandleRegister);
    server.Route("POST", "/api/auth/login", HandleLogin);
    server.Route("POST", "/api/auth/logout", HandleLogout);
    server.Route("GET", "/api/auth/me", HandleMe);
    server.Route("POST", "/api/auth/change-password", HandleChangePassword);

    // 物品
    server.Route("GET", "/api/items", HandleItemList);
    server.Route("POST", "/api/items", HandleItemCreate);
    server.Route("GET", "/api/items/{publicId}", HandleItemDetail);
    server.Route("PATCH", "/api/items/{publicId}", HandleItemUpdate);
    server.Route("DELETE", "/api/items/{publicId}", HandleItemDelete);
    server.Route("POST", "/api/items/{publicId}/images", HandleImageUpload);
    server.Route("DELETE", "/api/items/{publicId}/images/{imageId}", HandleImageDelete);
    server.Route("POST", "/api/items/{publicId}/claims", HandleClaimCreate);
    server.Route("GET", "/api/items/{publicId}/claims", HandleItemClaims);
    server.Route("GET", "/api/me/items", HandleMyItems);
    server.Route("GET", "/api/me/claims", HandleMyClaims);

    // 认领
    server.Route("POST", "/api/claims/{id}/approve",
                 [](const HttpRequest& req) { return HandleClaimAction(req, "approve"); });
    server.Route("POST", "/api/claims/{id}/reject",
                 [](const HttpRequest& req) { return HandleClaimAction(req, "reject"); });
    server.Route("POST", "/api/claims/{id}/cancel",
                 [](const HttpRequest& req) { return HandleClaimAction(req, "cancel"); });
    server.Route("POST", "/api/claims/{id}/verify",
                 [](const HttpRequest& req) { return HandleClaimAction(req, "verify"); });

    // 通知
    server.Route("GET", "/api/notifications", HandleNotifications);
    server.Route("POST", "/api/notifications/read", HandleNotificationsRead);

    // 举报
    server.Route("POST", "/api/reports", HandleReportCreate);

    // 管理后台
    server.Route("GET", "/api/admin/stats", HandleAdminStats);
    server.Route("GET", "/api/admin/reports", HandleAdminReports);
    server.Route("POST", "/api/admin/reports/{id}/handle", HandleAdminReportHandle);
    server.Route("GET", "/api/admin/invites", HandleAdminInvites);
    server.Route("POST", "/api/admin/invites", HandleAdminInviteCreate);
    server.Route("GET", "/api/admin/audit", HandleAdminAudit);
    server.Route("POST", "/api/admin/items/{publicId}/take-down", HandleAdminTakeDown);

    // 图片
    server.Route("GET", "/media/{fileName}", HandleMedia);

    // SSE 实时流
    server.RouteStream("GET", "/api/stream", HandleStream);

    // 兜底：非 API 的 GET 一律返回单页应用（hash 路由）
    server.SetFallback([](const HttpRequest& req) {
        if (req.method == "GET" && !StartsWith(req.path, "/api/") && !StartsWith(req.path, "/media/")) {
            return HttpResponse::Html(WebUiHtml());
        }
        return MakeError("NOT_FOUND", "接口不存在", 404);
    });
}

void Api::StartBackgroundTasks() {
    std::thread([]() { BackgroundLoop(); }).detach();
}

} // namespace lf