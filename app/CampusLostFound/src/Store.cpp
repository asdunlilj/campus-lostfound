// Store.cpp —— 校园失物招领系统数据仓库实现
//
// 设计要点：
//   * 单例；所有 public 方法都由 std::recursive_mutex 保护，内部实现函数以 Locked 结尾。
//   * 每次数据变更后调用 SaveLocked()：先写 db.json.tmp，再 rename 原子覆盖 db.json。
//   * 事件通过 SetEventSink 注入的 eventSink_ 发出，eventJson 可直接推给浏览器。
//     targetUserId = 0 表示广播，否则只发给该用户。
//   * 对外字符串一律 UTF-8 std::string；内部时间一律 epoch 秒，输出视图时再格式化。
//   * 只依赖 Windows 自带库（Auth/Util 内部用 BCrypt），不使用任何第三方库。

#include "Store.h"

#include "Auth.h"
#include "Json.h"
#include "Util.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace lf {
namespace {

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

constexpr int64_t kDbVersion = 1;
constexpr int64_t kDaySeconds = 86400;
constexpr int64_t kClaimTimeoutSeconds = 24 * 3600;  // pending 认领超过 24h 未审核自动过期
constexpr int64_t kPickupValidSeconds = 24 * 3600;   // 取件码 24h 有效
constexpr int64_t kDefaultItemValidSeconds = 30 * 86400;  // 发布默认 30 天有效
constexpr std::size_t kMaxAuditLogs = 5000;          // 审计日志最多保留条数
constexpr std::size_t kMaxItemImages = 3;            // 每个物品最多图片数

// 物品状态
constexpr int kItemOpen = 0;
constexpr int kItemClaimPending = 1;
constexpr int kItemClaimApproved = 2;
constexpr int kItemCompleted = 3;
constexpr int kItemExpired = 4;
constexpr int kItemRemoved = 5;

// 认领状态
constexpr int kClaimPending = 0;
constexpr int kClaimApproved = 1;
constexpr int kClaimRejected = 2;
constexpr int kClaimCancelled = 3;
constexpr int kClaimExpired = 4;
constexpr int kClaimCompleted = 5;

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------

std::string ToUpperAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return text;
}

std::vector<Category> DefaultCategories() {
    return {
        Category{1, "证件卡类", 1},
        Category{2, "电子产品", 2},
        Category{3, "钥匙", 3},
        Category{4, "书籍资料", 4},
        Category{5, "衣物", 5},
        Category{6, "运动器材", 6},
        Category{7, "其他", 7},
    };
}

std::vector<Location> DefaultLocations() {
    return {
        Location{1, "教学楼", "", 1},
        Location{2, "图书馆", "", 2},
        Location{3, "食堂", "", 3},
        Location{4, "宿舍区", "", 4},
        Location{5, "体育馆", "", 5},
        Location{6, "操场", "", 6},
        Location{7, "其他", "", 7},
    };
}

JsonValue MakeObj() { return JsonValue::MakeObject(); }
JsonValue MakeArr() { return JsonValue::MakeArray(); }

int64_t IntField(const JsonValue& value, const char* key, int64_t def = 0) {
    if (!value.IsObject()) return def;
    const JsonValue* field = value.Find(key);
    return field != nullptr ? field->AsInt(def) : def;
}

std::string StrField(const JsonValue& value, const char* key, const std::string& def = std::string()) {
    if (!value.IsObject()) return def;
    const JsonValue* field = value.Find(key);
    return field != nullptr ? field->AsString(def) : def;
}

bool BoolField(const JsonValue& value, const char* key, bool def = false) {
    if (!value.IsObject()) return def;
    const JsonValue* field = value.Find(key);
    return field != nullptr ? field->AsBool(def) : def;
}

int IntFieldInt(const JsonValue& value, const char* key, int def = 0) {
    return static_cast<int>(IntField(value, key, static_cast<int64_t>(def)));
}

// ---------------------------------------------------------------------------
// 序列化：结构体 -> JsonValue（db.json 与视图共用基础字段）
// ---------------------------------------------------------------------------

JsonValue CategoryToJson(const Category& category) {
    JsonValue value = MakeObj();
    value.Set("id", category.id);
    value.Set("name", category.name);
    value.Set("sort", category.sort);
    return value;
}

JsonValue LocationToJson(const Location& location) {
    JsonValue value = MakeObj();
    value.Set("id", location.id);
    value.Set("name", location.name);
    value.Set("building", location.building);
    value.Set("sort", location.sort);
    return value;
}

JsonValue UserToJson(const User& user) {
    JsonValue value = MakeObj();
    value.Set("id", user.id);
    value.Set("studentNo", user.studentNo);
    value.Set("name", user.name);
    value.Set("college", user.college);
    value.Set("passwordHash", user.passwordHash);
    value.Set("role", user.role);
    value.Set("status", user.status);
    value.Set("createdAt", user.createdAt);
    value.Set("lastLoginAt", user.lastLoginAt);
    return value;
}

JsonValue ItemImageToJson(const ItemImage& image) {
    JsonValue value = MakeObj();
    value.Set("id", image.id);
    value.Set("fileName", image.fileName);
    value.Set("sort", image.sort);
    return value;
}

JsonValue ItemToJson(const Item& item) {
    JsonValue value = MakeObj();
    value.Set("id", item.id);
    value.Set("publicId", item.publicId);
    value.Set("type", item.type);
    value.Set("userId", item.userId);
    value.Set("title", item.title);
    value.Set("description", item.description);
    value.Set("categoryId", item.categoryId);
    value.Set("locationId", item.locationId);
    value.Set("eventTime", item.eventTime);
    value.Set("status", item.status);
    value.Set("verifyQuestion", item.verifyQuestion);
    value.Set("expiresAt", item.expiresAt);
    value.Set("createdAt", item.createdAt);
    value.Set("updatedAt", item.updatedAt);
    value.Set("viewCount", item.viewCount);
    JsonValue images = MakeArr();
    for (const ItemImage& image : item.images) {
        images.Push(ItemImageToJson(image));
    }
    value.Set("images", std::move(images));
    value.Set("removedReason", item.removedReason);
    return value;
}

JsonValue ClaimToJson(const Claim& claim) {
    JsonValue value = MakeObj();
    value.Set("id", claim.id);
    value.Set("itemId", claim.itemId);
    value.Set("claimantId", claim.claimantId);
    value.Set("status", claim.status);
    value.Set("answer", claim.answer);
    value.Set("message", claim.message);
    value.Set("pickupCode", claim.pickupCode);
    value.Set("pickupExpiresAt", claim.pickupExpiresAt);
    value.Set("createdAt", claim.createdAt);
    value.Set("reviewedAt", claim.reviewedAt);
    value.Set("completedAt", claim.completedAt);
    return value;
}

JsonValue NotificationToJson(const Notification& notification) {
    JsonValue value = MakeObj();
    value.Set("id", notification.id);
    value.Set("userId", notification.userId);
    value.Set("type", notification.type);
    value.Set("title", notification.title);
    value.Set("body", notification.body);
    value.Set("link", notification.link);
    value.Set("read", notification.read);
    value.Set("createdAt", notification.createdAt);
    return value;
}

// API_CONTRACT.md 1.4 NotificationView
JsonValue NotificationViewToJson(const Notification& notification) {
    JsonValue value = MakeObj();
    value.Set("id", notification.id);
    value.Set("type", notification.type);
    value.Set("title", notification.title);
    value.Set("body", notification.body);
    value.Set("link", notification.link);
    value.Set("read", notification.read);
    value.Set("createdAt", FormatIso8601(notification.createdAt));
    value.Set("createdAtText", FormatTimeAgo(notification.createdAt));
    return value;
}

JsonValue ReportToJson(const Report& report) {
    JsonValue value = MakeObj();
    value.Set("id", report.id);
    value.Set("reporterId", report.reporterId);
    value.Set("targetType", report.targetType);
    value.Set("targetItemId", report.targetItemId);
    value.Set("reason", report.reason);
    value.Set("status", report.status);
    value.Set("handledBy", report.handledBy);
    value.Set("handledAt", report.handledAt);
    value.Set("createdAt", report.createdAt);
    return value;
}

JsonValue AuditToJson(const AuditLog& audit) {
    JsonValue value = MakeObj();
    value.Set("id", audit.id);
    value.Set("actorId", audit.actorId);
    value.Set("action", audit.action);
    value.Set("detail", audit.detail);
    value.Set("ip", audit.ip);
    value.Set("createdAt", audit.createdAt);
    return value;
}

JsonValue InviteToJson(const Invite& invite) {
    JsonValue value = MakeObj();
    value.Set("codeHash", invite.codeHash);
    value.Set("codeMasked", invite.codeMasked);
    value.Set("maxUses", invite.maxUses);
    value.Set("uses", invite.uses);
    value.Set("expiresAt", invite.expiresAt);
    value.Set("disabled", invite.disabled);
    value.Set("createdBy", invite.createdBy);
    value.Set("createdAt", invite.createdAt);
    return value;
}

// ---------------------------------------------------------------------------
// 反序列化：JsonValue -> 结构体（缺字段一律使用默认值，保证旧数据可读）
// ---------------------------------------------------------------------------

Category CategoryFromJson(const JsonValue& value) {
    Category category;
    category.id = IntFieldInt(value, "id", 0);
    category.name = StrField(value, "name");
    category.sort = IntFieldInt(value, "sort", 0);
    return category;
}

Location LocationFromJson(const JsonValue& value) {
    Location location;
    location.id = IntFieldInt(value, "id", 0);
    location.name = StrField(value, "name");
    location.building = StrField(value, "building");
    location.sort = IntFieldInt(value, "sort", 0);
    return location;
}

User UserFromJson(const JsonValue& value) {
    User user;
    user.id = IntField(value, "id", 0);
    user.studentNo = StrField(value, "studentNo");
    user.name = StrField(value, "name");
    user.college = StrField(value, "college");
    user.passwordHash = StrField(value, "passwordHash");
    user.role = StrField(value, "role", "student");
    if (user.role != "admin") user.role = "student";
    user.status = IntFieldInt(value, "status", 1);
    user.createdAt = IntField(value, "createdAt", 0);
    user.lastLoginAt = IntField(value, "lastLoginAt", 0);
    return user;
}

ItemImage ItemImageFromJson(const JsonValue& value) {
    ItemImage image;
    image.id = StrField(value, "id");
    image.fileName = StrField(value, "fileName");
    image.sort = IntFieldInt(value, "sort", 0);
    return image;
}

Item ItemFromJson(const JsonValue& value) {
    Item item;
    item.id = IntField(value, "id", 0);
    item.publicId = StrField(value, "publicId");
    item.type = IntFieldInt(value, "type", 1);
    if (item.type != 0 && item.type != 1) item.type = 1;
    item.userId = IntField(value, "userId", 0);
    item.title = StrField(value, "title");
    item.description = StrField(value, "description");
    item.categoryId = IntFieldInt(value, "categoryId", 0);
    item.locationId = IntFieldInt(value, "locationId", 0);
    item.eventTime = StrField(value, "eventTime");
    item.status = IntFieldInt(value, "status", 0);
    if (item.status < 0 || item.status > 5) item.status = 0;
    item.verifyQuestion = StrField(value, "verifyQuestion");
    item.expiresAt = IntField(value, "expiresAt", 0);
    item.createdAt = IntField(value, "createdAt", 0);
    item.updatedAt = IntField(value, "updatedAt", item.createdAt);
    item.viewCount = IntFieldInt(value, "viewCount", 0);
    if (item.viewCount < 0) item.viewCount = 0;
    item.removedReason = StrField(value, "removedReason");
    const JsonValue* images = value.Find("images");
    if (images != nullptr && images->IsArray()) {
        for (const JsonValue& imageValue : images->Items()) {
            if (!imageValue.IsObject()) continue;
            item.images.push_back(ItemImageFromJson(imageValue));
        }
    }
    return item;
}

Claim ClaimFromJson(const JsonValue& value) {
    Claim claim;
    claim.id = IntField(value, "id", 0);
    claim.itemId = IntField(value, "itemId", 0);
    claim.claimantId = IntField(value, "claimantId", 0);
    claim.status = IntFieldInt(value, "status", 0);
    if (claim.status < 0 || claim.status > 5) claim.status = 0;
    claim.answer = StrField(value, "answer");
    claim.message = StrField(value, "message");
    claim.pickupCode = StrField(value, "pickupCode");
    claim.pickupExpiresAt = IntField(value, "pickupExpiresAt", 0);
    claim.createdAt = IntField(value, "createdAt", 0);
    claim.reviewedAt = IntField(value, "reviewedAt", 0);
    claim.completedAt = IntField(value, "completedAt", 0);
    return claim;
}

Notification NotificationFromJson(const JsonValue& value) {
    Notification notification;
    notification.id = IntField(value, "id", 0);
    notification.userId = IntField(value, "userId", 0);
    notification.type = StrField(value, "type");
    notification.title = StrField(value, "title");
    notification.body = StrField(value, "body");
    notification.link = StrField(value, "link");
    notification.read = BoolField(value, "read", false);
    notification.createdAt = IntField(value, "createdAt", 0);
    return notification;
}

Report ReportFromJson(const JsonValue& value) {
    Report report;
    report.id = IntField(value, "id", 0);
    report.reporterId = IntField(value, "reporterId", 0);
    report.targetType = StrField(value, "targetType", "item");
    report.targetItemId = IntField(value, "targetItemId", 0);
    report.reason = StrField(value, "reason");
    report.status = IntFieldInt(value, "status", 0);
    if (report.status < 0 || report.status > 2) report.status = 0;
    report.handledBy = IntField(value, "handledBy", 0);
    report.handledAt = IntField(value, "handledAt", 0);
    report.createdAt = IntField(value, "createdAt", 0);
    return report;
}

AuditLog AuditFromJson(const JsonValue& value) {
    AuditLog audit;
    audit.id = IntField(value, "id", 0);
    audit.actorId = IntField(value, "actorId", 0);
    audit.action = StrField(value, "action");
    audit.detail = StrField(value, "detail");
    audit.ip = StrField(value, "ip");
    audit.createdAt = IntField(value, "createdAt", 0);
    return audit;
}

Invite InviteFromJson(const JsonValue& value) {
    Invite invite;
    invite.codeHash = StrField(value, "codeHash");
    invite.codeMasked = StrField(value, "codeMasked");
    // 兼容旧版本：旧数据里的明文 code 迁移为哈希 + 掩码（下次保存后明文消失）
    if (invite.codeHash.empty()) {
        const std::string legacyCode = StrField(value, "code");
        if (!legacyCode.empty()) {
            invite.codeHash = Sha256Hex(ToUpperAscii(Trim(legacyCode)));
            invite.codeMasked = legacyCode.substr(0, 7) + "****";
            // 旧版本明文邀请码可能已经泄露（例如曾被打印/公开），升级后一律停用，需管理员重新创建
            invite.disabled = true;
        }
    }
    invite.maxUses = IntFieldInt(value, "maxUses", 0);
    invite.uses = IntFieldInt(value, "uses", 0);
    invite.expiresAt = IntField(value, "expiresAt", 0);
    invite.disabled = BoolField(value, "disabled", false);
    invite.createdBy = StrField(value, "createdBy");
    invite.createdAt = IntField(value, "createdAt", 0);
    return invite;
}

// ---------------------------------------------------------------------------
// 容器查找 / 批量加载辅助
// ---------------------------------------------------------------------------

template <typename T>
T* FindById(std::vector<T>& list, int64_t id) {
    for (T& element : list) {
        if (static_cast<int64_t>(element.id) == id) return &element;
    }
    return nullptr;
}

template <typename T>
const T* FindById(const std::vector<T>& list, int64_t id) {
    for (const T& element : list) {
        if (static_cast<int64_t>(element.id) == id) return &element;
    }
    return nullptr;
}

template <typename T>
int64_t MaxId(const std::vector<T>& list) {
    int64_t maxId = 0;
    for (const T& element : list) {
        const int64_t id = static_cast<int64_t>(element.id);
        if (id > maxId) maxId = id;
    }
    return maxId;
}

template <typename T, typename Convert>
void LoadArrayField(const JsonValue& root, const char* key, std::vector<T>& out, Convert convert) {
    out.clear();
    const JsonValue* array = root.Find(key);
    if (array == nullptr || !array->IsArray()) return;
    for (const JsonValue& element : array->Items()) {
        if (!element.IsObject()) continue;
        out.push_back(convert(element));
    }
}

std::string NormalizeInviteCode(const std::string& code) {
    return ToUpperAscii(Trim(code));
}

std::string HashInviteCode(const std::string& code) {
    const std::string normalized = NormalizeInviteCode(code);
    if (normalized.empty()) return std::string();
    return Sha256Hex(normalized);
}

Invite* FindInviteByCode(std::vector<Invite>& invites, const std::string& code) {
    const std::string hash = HashInviteCode(code);
    if (hash.empty()) return nullptr;
    for (Invite& invite : invites) {
        if (!invite.codeHash.empty() && ConstantTimeEquals(invite.codeHash, hash)) return &invite;
    }
    return nullptr;
}

const Invite* FindInviteByCode(const std::vector<Invite>& invites, const std::string& code) {
    const std::string hash = HashInviteCode(code);
    if (hash.empty()) return nullptr;
    for (const Invite& invite : invites) {
        if (!invite.codeHash.empty() && ConstantTimeEquals(invite.codeHash, hash)) return &invite;
    }
    return nullptr;
}

bool PublicIdExists(const std::vector<Item>& items, const std::string& publicId) {
    if (publicId.empty()) return false;
    for (const Item& item : items) {
        if (item.publicId == publicId) return true;
    }
    return false;
}

std::string MaskedNameOf(const std::vector<User>& users, int64_t userId) {
    const User* user = FindById(users, userId);
    if (user == nullptr || user->name.empty()) return "匿名用户";
    return MaskName(user->name);
}

Notification PushNotification(std::vector<Notification>& list, int64_t& sequence, int64_t userId,
                              const std::string& type, const std::string& title,
                              const std::string& body, const std::string& link) {
    Notification notification;
    notification.id = ++sequence;
    notification.userId = userId;
    notification.type = type;
    notification.title = title;
    notification.body = body;
    notification.link = link;
    notification.read = false;
    notification.createdAt = NowEpoch();
    list.push_back(notification);
    return notification;
}

// 生成 6 位数字取件码（基于 BCryptGenRandom 的随机十六进制，映射成十进制数字）
std::string GeneratePickupCode() {
    const std::string hex = RandomHex(6);
    std::string code;
    code.reserve(6);
    for (char c : hex) {
        int digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        }
        code.push_back(static_cast<char>('0' + (digit % 10)));
        if (code.size() == 6) break;
    }
    while (code.size() < 6) code.push_back('0');
    return code;
}

std::string ItemStatusText(int status) {
    switch (status) {
        case kItemOpen: return "进行中";
        case kItemClaimPending: return "认领待审核";
        case kItemClaimApproved: return "已通过待交接";
        case kItemCompleted: return "已完成";
        case kItemExpired: return "已过期";
        case kItemRemoved: return "已下架";
        default: return "未知状态";
    }
}

// ---------------------------------------------------------------------------
// SSE 事件 JSON 构造（字段与 API_CONTRACT.md §9 一致）
// ---------------------------------------------------------------------------

std::string EventItemCreatedJson(const Item& item) {
    JsonValue event = MakeObj();
    event.Set("type", "item-created");
    event.Set("publicId", item.publicId);
    event.Set("itemType", item.type);
    event.Set("title", item.title);
    return event.Dump();
}

std::string EventItemUpdatedJson(const Item& item) {
    JsonValue event = MakeObj();
    event.Set("type", "item-updated");
    event.Set("publicId", item.publicId);
    event.Set("status", item.status);
    return event.Dump();
}

std::string EventClaimCreatedJson(const Claim& claim, const Item& item, const std::string& claimantName) {
    JsonValue event = MakeObj();
    event.Set("type", "claim-created");
    event.Set("claimId", claim.id);
    event.Set("itemPublicId", item.publicId);
    event.Set("itemTitle", item.title);
    event.Set("claimantName", claimantName);
    return event.Dump();
}

std::string EventClaimUpdatedJson(const Claim& claim, const Item& item) {
    JsonValue event = MakeObj();
    event.Set("type", "claim-updated");
    event.Set("claimId", claim.id);
    event.Set("itemPublicId", item.publicId);
    event.Set("status", claim.status);
    return event.Dump();
}

} // namespace

// ===========================================================================
// Store 单例与生命周期
// ===========================================================================

Store& Store::Instance() {
    static Store instance;
    return instance;
}

bool Store::Load(const std::string& dataDir, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return LoadLocked(dataDir, error);
}

bool Store::Save(std::string* errorOut) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return SaveLocked(errorOut);
}

const std::string& Store::DataDir() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return dataDir_;
}

void Store::SetEventSink(std::function<void(const std::string&, int64_t)> sink) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    eventSink_ = std::move(sink);
}

const std::vector<Category>& Store::Categories() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return categories_;
}

const std::vector<Location>& Store::Locations() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return locations_;
}

// ===========================================================================
// 用户
// ===========================================================================

std::optional<User> Store::FindUserById(int64_t id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const User* user = FindById(users_, id);
    if (user == nullptr) return std::nullopt;
    return *user;
}

std::optional<User> Store::FindUserByStudentNo(const std::string& studentNo) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string target = ToLowerAscii(Trim(studentNo));
    if (target.empty()) return std::nullopt;
    for (const User& user : users_) {
        if (ToLowerAscii(user.studentNo) == target) return user;
    }
    return std::nullopt;
}

std::vector<User> Store::AllUsers() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return users_;
}

int64_t Store::CreateUser(const std::string& studentNo, const std::string& name, const std::string& college,
                          const std::string& passwordHash, const std::string& role, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    const std::string normalizedNo = Trim(studentNo);
    const std::string normalizedName = Trim(name);
    if (normalizedNo.empty()) {
        error = "学号不能为空";
        return 0;
    }
    if (normalizedName.empty()) {
        error = "姓名不能为空";
        return 0;
    }
    if (FindUserByStudentNo(normalizedNo).has_value()) {
        error = "该学号已注册";
        return 0;
    }
    User user;
    user.id = ++seqUser_;
    user.studentNo = normalizedNo;
    user.name = normalizedName;
    user.college = Trim(college);
    user.passwordHash = passwordHash;
    user.role = ToLowerAscii(Trim(role)) == "admin" ? "admin" : "student";
    user.status = 1;
    user.createdAt = NowEpoch();
    user.lastLoginAt = 0;
    users_.push_back(user);
    SaveLocked(nullptr);
    return user.id;
}

bool Store::UpdateLastLogin(int64_t userId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    User* user = FindById(users_, userId);
    if (user == nullptr) return false;
    user->lastLoginAt = NowEpoch();
    SaveLocked(nullptr);
    return true;
}

bool Store::ChangePassword(int64_t userId, const std::string& newHash) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    User* user = FindById(users_, userId);
    if (user == nullptr) return false;
    user->passwordHash = newHash;
    SaveLocked(nullptr);
    return true;
}

// ===========================================================================
// 邀请码
// ===========================================================================

std::vector<Invite> Store::AllInvites() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return invites_;
}

std::optional<Invite> Store::FindInvite(const std::string& code) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const Invite* invite = FindInviteByCode(invites_, code);
    if (invite == nullptr) return std::nullopt;
    return *invite;
}

std::string Store::TakeInitialAdminPassword() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string password = initialAdminPassword_;
    initialAdminPassword_.clear();
    return password;
}

Invite Store::CreateInvite(int maxUses, int validDays, const std::string& createdBy, std::string& fullCode) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Invite invite;
    int useLimit = maxUses;
    if (useLimit <= 0) useLimit = 1;
    if (useLimit > 100000) useLimit = 100000;
    int days = validDays;
    if (days <= 0) days = 30;
    if (days > 3650) days = 3650;
    std::string code;
    do {
        code = "LF-" + ToUpperAscii(RandomHex(8)).substr(0, 12);
    } while (FindInviteByCode(invites_, code) != nullptr);
    const int64_t now = NowEpoch();
    fullCode = code;
    invite.codeHash = HashInviteCode(code);
    invite.codeMasked = code.substr(0, 7) + "****";
    invite.maxUses = useLimit;
    invite.uses = 0;
    invite.expiresAt = now + static_cast<int64_t>(days) * kDaySeconds;
    invite.disabled = false;
    invite.createdBy = createdBy;
    invite.createdAt = now;
    invites_.push_back(invite);
    SaveLocked(nullptr);
    return invite;
}

bool Store::ConsumeInvite(const std::string& code, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Invite* invite = FindInviteByCode(invites_, code);
    if (invite == nullptr) {
        error = "邀请码不存在";
        return false;
    }
    if (invite->disabled) {
        error = "邀请码已停用";
        return false;
    }
    const int64_t now = NowEpoch();
    if (invite->expiresAt > 0 && invite->expiresAt <= now) {
        error = "邀请码已过期";
        return false;
    }
    if (invite->uses >= invite->maxUses) {
        error = "邀请码使用次数已达上限";
        return false;
    }
    ++invite->uses;
    SaveLocked(nullptr);
    return true;
}

// ===========================================================================
// 物品
// ===========================================================================

std::vector<Item> Store::QueryItems(int type, int categoryId, int locationId, const std::string& statusFilter,
                                    const std::string& keyword, int offset, int limit, int& total) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string filter = Trim(statusFilter);
    const std::string keywordLower = ToLowerAscii(Trim(keyword));
    const bool filterType = (type == 0 || type == 1);
    const bool filterCategory = (categoryId > 0);
    const bool filterLocation = (locationId > 0);
    const bool filterKeyword = !keywordLower.empty();

    auto statusMatches = [&filter](int status) -> bool {
        if (filter.empty() || filter == "active") {
            return status == kItemOpen || status == kItemClaimPending || status == kItemClaimApproved;
        }
        if (filter == "open") return status == kItemOpen;
        if (filter == "claim_pending") return status == kItemClaimPending;
        if (filter == "claim_approved") return status == kItemClaimApproved;
        if (filter == "completed") return status == kItemCompleted;
        if (filter == "expired") return status == kItemExpired;
        if (filter == "removed") return status == kItemRemoved;
        return true;  // "all" 及未知值一律不过滤
    };

    std::vector<Item> matched;
    for (const Item& item : items_) {
        if (filterType && item.type != type) continue;
        if (filterCategory && item.categoryId != categoryId) continue;
        if (filterLocation && item.locationId != locationId) continue;
        if (!statusMatches(item.status)) continue;
        if (filterKeyword) {
            const bool inTitle = ToLowerAscii(item.title).find(keywordLower) != std::string::npos;
            const bool inDescription = ToLowerAscii(item.description).find(keywordLower) != std::string::npos;
            if (!inTitle && !inDescription) continue;
        }
        matched.push_back(item);
    }
    std::sort(matched.begin(), matched.end(), [](const Item& a, const Item& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    total = static_cast<int>(matched.size());
    int start = offset > 0 ? offset : 0;
    if (start >= total) return std::vector<Item>();
    std::size_t end = matched.size();
    if (limit > 0) {
        const std::size_t wanted = static_cast<std::size_t>(start) + static_cast<std::size_t>(limit);
        if (wanted < end) end = wanted;
    }
    return std::vector<Item>(matched.begin() + start, matched.begin() + static_cast<std::ptrdiff_t>(end));
}

std::optional<Item> Store::FindItemById(int64_t id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const Item* item = FindById(items_, id);
    if (item == nullptr) return std::nullopt;
    return *item;
}

std::optional<Item> Store::FindItemByPublicId(const std::string& publicId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (publicId.empty()) return std::nullopt;
    for (const Item& item : items_) {
        if (item.publicId == publicId) return item;
    }
    return std::nullopt;
}

bool Store::CreateItem(Item& item, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    const int64_t now = NowEpoch();
    item.id = ++seqItem_;
    if (item.publicId.empty()) item.publicId = NewPublicId();
    while (PublicIdExists(items_, item.publicId)) {
        item.publicId = NewPublicId();
    }
    item.status = kItemOpen;
    item.createdAt = now;
    item.updatedAt = now;
    if (item.expiresAt <= 0) item.expiresAt = now + kDefaultItemValidSeconds;
    if (item.viewCount < 0) item.viewCount = 0;
    items_.push_back(item);
    SaveLocked(nullptr);
    EmitLocked(EventItemCreatedJson(item), 0);
    return true;
}

bool Store::UpdateItem(int64_t itemId, const std::string& title, const std::string& description,
                       int categoryId, int locationId, const std::string& eventTime,
                       const std::string& verifyQuestion, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Item* item = FindById(items_, itemId);
    if (item == nullptr) {
        error = "物品不存在";
        return false;
    }
    if (item->status != kItemOpen) {
        error = "当前状态下不可编辑物品信息";
        return false;
    }
    item->title = title;
    item->description = description;
    item->categoryId = categoryId;
    item->locationId = locationId;
    item->eventTime = eventTime;
    item->verifyQuestion = verifyQuestion;
    item->updatedAt = NowEpoch();
    SaveLocked(nullptr);
    EmitLocked(EventItemUpdatedJson(*item), 0);
    return true;
}

bool Store::SetItemStatus(int64_t itemId, int status, const std::string& reason, int64_t actorId, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    if (status < kItemOpen || status > kItemRemoved) {
        error = "物品状态无效";
        return false;
    }
    Item* item = FindById(items_, itemId);
    if (item == nullptr) {
        error = "物品不存在";
        return false;
    }
    if (item->status == status && (status != kItemRemoved || item->removedReason == reason)) {
        return true;
    }
    const int64_t now = NowEpoch();
    item->status = status;
    item->updatedAt = now;
    if (status == kItemRemoved) {
        item->removedReason = reason;
    } else if (status == kItemOpen) {
        item->removedReason.clear();
    }

    std::vector<Notification> pendingNotifications;
    std::vector<std::pair<std::string, int64_t>> pendingEvents;
    if (status == kItemRemoved) {
        // 下架/删除后，所有进行中的认领申请自动关闭，避免“物品已下架但认领仍可审核/核销”的悬空状态
        for (Claim& claim : claims_) {
            if (claim.itemId != item->id) continue;
            if (claim.status != kClaimPending && claim.status != kClaimApproved) continue;
            claim.status = kClaimExpired;
            claim.reviewedAt = now;
            const std::string claimEvent = EventClaimUpdatedJson(claim, *item);
            pendingEvents.push_back(std::make_pair(claimEvent, claim.claimantId));
            pendingEvents.push_back(std::make_pair(claimEvent, item->userId));
            pendingNotifications.push_back(PushNotification(
                notifications_, seqNotification_, claim.claimantId, "claim", "认领申请已关闭",
                "「" + item->title + "」已被下架，你的认领申请已关闭。", "#/item/" + item->publicId));
        }
        // 仅当操作者不是发布者本人时，才给发布者发“被下架”通知（自己删除不再误发管理员下架通知）
        if (actorId != item->userId) {
            std::string body = "「" + item->title + "」已被下架";
            if (!reason.empty()) body += "，原因：" + reason;
            body += "。";
            pendingNotifications.push_back(PushNotification(
                notifications_, seqNotification_, item->userId, "item", "物品已被下架", body,
                "#/item/" + item->publicId));
        }
    }
    SaveLocked(nullptr);
    EmitLocked(EventItemUpdatedJson(*item), 0);
    for (const auto& event : pendingEvents) EmitLocked(event.first, event.second);
    for (const Notification& notification : pendingNotifications) EmitNotificationLocked(notification);
    return true;
}

void Store::IncrementView(int64_t itemId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Item* item = FindById(items_, itemId);
    if (item == nullptr) return;
    ++item->viewCount;
    SaveLocked(nullptr);
}

bool Store::AddItemImage(int64_t itemId, const std::string& fileName, std::string& imageId, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Item* item = FindById(items_, itemId);
    if (item == nullptr) {
        error = "物品不存在";
        return false;
    }
    if (Trim(fileName).empty()) {
        error = "图片文件名不能为空";
        return false;
    }
    if (item->images.size() >= kMaxItemImages) {
        error = "每个物品最多上传 3 张图片";
        return false;
    }
    std::string newId = imageId.empty() ? ("img" + RandomHex(6)) : imageId;
    bool conflict = true;
    while (conflict) {
        conflict = false;
        for (const ItemImage& image : item->images) {
            if (image.id == newId) {
                conflict = true;
                break;
            }
        }
        if (conflict) newId = "img" + RandomHex(6);
    }
    ItemImage image;
    image.id = newId;
    image.fileName = fileName;
    image.sort = static_cast<int>(item->images.size());
    item->images.push_back(image);
    item->updatedAt = NowEpoch();
    imageId = newId;
    SaveLocked(nullptr);
    EmitLocked(EventItemUpdatedJson(*item), 0);
    return true;
}

bool Store::RemoveItemImage(int64_t itemId, const std::string& imageId, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Item* item = FindById(items_, itemId);
    if (item == nullptr) {
        error = "物品不存在";
        return false;
    }
    std::vector<ItemImage>& images = item->images;
    auto position = std::find_if(images.begin(), images.end(), [&imageId](const ItemImage& image) {
        return image.id == imageId;
    });
    if (position == images.end()) {
        error = "图片不存在";
        return false;
    }
    images.erase(position);
    for (std::size_t index = 0; index < images.size(); ++index) {
        images[index].sort = static_cast<int>(index);
    }
    item->updatedAt = NowEpoch();
    SaveLocked(nullptr);
    EmitLocked(EventItemUpdatedJson(*item), 0);
    return true;
}

// ===========================================================================
// 认领
// ===========================================================================

std::optional<Claim> Store::FindClaimById(int64_t id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const Claim* claim = FindById(claims_, id);
    if (claim == nullptr) return std::nullopt;
    return *claim;
}

std::vector<Claim> Store::ClaimsForItem(int64_t itemId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Claim> result;
    for (const Claim& claim : claims_) {
        if (claim.itemId == itemId) result.push_back(claim);
    }
    std::sort(result.begin(), result.end(), [](const Claim& a, const Claim& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    return result;
}

std::vector<Claim> Store::ClaimsByUser(int64_t userId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Claim> result;
    for (const Claim& claim : claims_) {
        if (claim.claimantId == userId) result.push_back(claim);
    }
    std::sort(result.begin(), result.end(), [](const Claim& a, const Claim& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    return result;
}

int Store::CountClaimsForItem(int64_t itemId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int count = 0;
    for (const Claim& claim : claims_) {
        if (claim.itemId == itemId) ++count;
    }
    return count;
}

bool Store::CreateClaim(Claim& claim, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Item* item = FindById(items_, claim.itemId);
    if (item == nullptr) {
        error = "物品不存在";
        return false;
    }
    if (item->status != kItemOpen) {
        error = "该物品当前不可认领";
        return false;
    }
    if (item->expiresAt > 0 && item->expiresAt <= NowEpoch()) {
        error = "物品已过期，无法认领";
        return false;
    }
    if (claim.claimantId <= 0) {
        error = "请先登录后再认领";
        return false;
    }
    if (claim.claimantId == item->userId) {
        error = "不能认领自己发布的物品";
        return false;
    }
    if (!Trim(item->verifyQuestion).empty() && Trim(claim.answer).empty()) {
        error = "请回答物品的核验问题";
        return false;
    }
    for (const Claim& existing : claims_) {
        if (existing.itemId == item->id &&
            (existing.status == kClaimPending || existing.status == kClaimApproved)) {
            error = "该物品已有进行中的认领申请";
            return false;
        }
    }
    const int64_t now = NowEpoch();
    claim.id = ++seqClaim_;
    claim.status = kClaimPending;
    claim.pickupCode.clear();
    claim.pickupExpiresAt = 0;
    claim.createdAt = now;
    claim.reviewedAt = 0;
    claim.completedAt = 0;
    claims_.push_back(claim);
    item->status = kItemClaimPending;
    item->updatedAt = now;
    const std::string claimantName = MaskedNameOf(users_, claim.claimantId);
    Notification notification = PushNotification(
        notifications_, seqNotification_, item->userId, "claim", "收到新的认领申请",
        "「" + item->title + "」收到认领申请", "#/item/" + item->publicId);
    SaveLocked(nullptr);
    EmitLocked(EventClaimCreatedJson(claim, *item, claimantName), item->userId);
    EmitLocked(EventItemUpdatedJson(*item), 0);
    EmitNotificationLocked(notification);
    return true;
}

bool Store::ApproveClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Claim* claim = FindById(claims_, claimId);
    if (claim == nullptr) {
        error = "认领申请不存在";
        return false;
    }
    if (claim->status != kClaimPending) {
        error = "该认领申请当前不可审核";
        return false;
    }
    Item* item = FindById(items_, claim->itemId);
    if (item == nullptr) {
        error = "关联的物品不存在";
        return false;
    }
    const User* actor = FindById(users_, actorId);
    if (actor == nullptr) {
        error = "操作用户不存在";
        return false;
    }
    if (item->userId != actorId && actor->role != "admin") {
        error = "无权审核该认领申请";
        return false;
    }
    if (item->status != kItemClaimPending) {
        error = "物品当前状态不可审核认领";
        return false;
    }
    const int64_t now = NowEpoch();
    if (item->expiresAt > 0 && item->expiresAt <= now) {
        error = "物品已过期，无法通过认领";
        return false;
    }
    if (claim->createdAt > 0 && now - claim->createdAt >= kClaimTimeoutSeconds) {
        error = "认领申请已超时，请让申请人重新提交";
        return false;
    }
    claim->status = kClaimApproved;
    claim->reviewedAt = now;
    claim->pickupCode = GeneratePickupCode();
    claim->pickupExpiresAt = now + kPickupValidSeconds;
    item->status = kItemClaimApproved;
    item->updatedAt = now;
    const std::string itemLink = "#/item/" + item->publicId;

    std::vector<Notification> pendingNotifications;
    std::vector<std::pair<std::string, int64_t>> pendingEvents;
    for (Claim& other : claims_) {
        if (&other == claim) continue;
        if (other.itemId != item->id) continue;
        if (other.status != kClaimPending) continue;
        other.status = kClaimRejected;
        other.reviewedAt = now;
        pendingNotifications.push_back(PushNotification(
            notifications_, seqNotification_, other.claimantId, "claim", "认领申请未通过",
            "「" + item->title + "」已由其他同学认领，你的申请已自动关闭。", itemLink));
        const std::string eventJson = EventClaimUpdatedJson(other, *item);
        pendingEvents.push_back(std::make_pair(eventJson, other.claimantId));
        pendingEvents.push_back(std::make_pair(eventJson, item->userId));
    }
    pendingNotifications.push_back(PushNotification(
        notifications_, seqNotification_, claim->claimantId, "claim", "认领申请已通过",
        "「" + item->title + "」的认领申请已通过，取件码：" + claim->pickupCode + "（24 小时内有效）。", itemLink));
    const std::string claimEvent = EventClaimUpdatedJson(*claim, *item);
    const std::string itemEvent = EventItemUpdatedJson(*item);
    out = *claim;
    SaveLocked(nullptr);
    EmitLocked(itemEvent, 0);
    for (const auto& event : pendingEvents) EmitLocked(event.first, event.second);
    EmitLocked(claimEvent, claim->claimantId);
    EmitLocked(claimEvent, item->userId);
    for (const Notification& notification : pendingNotifications) EmitNotificationLocked(notification);
    return true;
}

bool Store::RejectClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Claim* claim = FindById(claims_, claimId);
    if (claim == nullptr) {
        error = "认领申请不存在";
        return false;
    }
    if (claim->status != kClaimPending) {
        error = "该认领申请当前不可审核";
        return false;
    }
    Item* item = FindById(items_, claim->itemId);
    if (item == nullptr) {
        error = "关联的物品不存在";
        return false;
    }
    const User* actor = FindById(users_, actorId);
    if (actor == nullptr) {
        error = "操作用户不存在";
        return false;
    }
    if (item->userId != actorId && actor->role != "admin") {
        error = "无权审核该认领申请";
        return false;
    }
    const int64_t now = NowEpoch();
    claim->status = kClaimRejected;
    claim->reviewedAt = now;
    bool itemChanged = false;
    if (item->status == kItemClaimPending) {
        item->status = kItemOpen;
        item->updatedAt = now;
        itemChanged = true;
    }
    Notification notification = PushNotification(notifications_, seqNotification_, claim->claimantId, "claim",
                                                "认领申请未通过", "「" + item->title + "」的认领申请未通过。",
                                                "#/item/" + item->publicId);
    const std::string claimEvent = EventClaimUpdatedJson(*claim, *item);
    const std::string itemEvent = itemChanged ? EventItemUpdatedJson(*item) : std::string();
    out = *claim;
    SaveLocked(nullptr);
    EmitLocked(claimEvent, claim->claimantId);
    EmitLocked(claimEvent, item->userId);
    if (itemChanged) EmitLocked(itemEvent, 0);
    EmitNotificationLocked(notification);
    return true;
}

bool Store::CancelClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Claim* claim = FindById(claims_, claimId);
    if (claim == nullptr) {
        error = "认领申请不存在";
        return false;
    }
    if (claim->claimantId != actorId) {
        error = "只能取消自己的认领申请";
        return false;
    }
    if (claim->status != kClaimPending) {
        error = "该认领申请当前不可取消";
        return false;
    }
    Item* item = FindById(items_, claim->itemId);
    const int64_t now = NowEpoch();
    claim->status = kClaimCancelled;
    claim->reviewedAt = now;
    bool itemChanged = false;
    if (item != nullptr && item->status == kItemClaimPending) {
        item->status = kItemOpen;
        item->updatedAt = now;
        itemChanged = true;
    }
    std::string claimEvent;
    std::string itemEvent;
    Notification notification;
    if (item != nullptr) {
        claimEvent = EventClaimUpdatedJson(*claim, *item);
        if (itemChanged) itemEvent = EventItemUpdatedJson(*item);
        notification = PushNotification(notifications_, seqNotification_, item->userId, "claim", "认领申请已取消",
                                        "「" + item->title + "」的认领申请已被认领人取消。", "#/item/" + item->publicId);
    }
    out = *claim;
    SaveLocked(nullptr);
    if (item != nullptr) {
        EmitLocked(claimEvent, claim->claimantId);
        EmitLocked(claimEvent, item->userId);
        if (itemChanged) EmitLocked(itemEvent, 0);
        EmitNotificationLocked(notification);
    }
    return true;
}

bool Store::VerifyClaim(int64_t claimId, int64_t actorId, const std::string& code, Claim& out, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    Claim* claim = FindById(claims_, claimId);
    if (claim == nullptr) {
        error = "认领申请不存在";
        return false;
    }
    if (claim->status != kClaimApproved) {
        error = "该认领申请当前不可核销";
        return false;
    }
    Item* item = FindById(items_, claim->itemId);
    if (item == nullptr) {
        error = "关联的物品不存在";
        return false;
    }
    if (item->status != kItemClaimApproved) {
        error = "物品当前状态不可核销";
        return false;
    }
    const User* actor = FindById(users_, actorId);
    if (actor == nullptr) {
        error = "操作用户不存在";
        return false;
    }
    if (item->userId != actorId && actor->role != "admin") {
        error = "无权核销该认领申请";
        return false;
    }
    const int64_t now = NowEpoch();
    if (claim->pickupExpiresAt > 0 && now > claim->pickupExpiresAt) {
        error = "取件码已过期";
        return false;
    }
    if (claim->pickupCode.empty()) {
        error = "该认领申请没有取件码";
        return false;
    }
    if (!ConstantTimeEquals(Trim(code), claim->pickupCode)) {
        error = "取件码不正确";
        return false;
    }
    claim->status = kClaimCompleted;
    claim->completedAt = now;
    item->status = kItemCompleted;
    item->updatedAt = now;
    const std::string itemLink = "#/item/" + item->publicId;
    std::vector<Notification> pendingNotifications;
    pendingNotifications.push_back(PushNotification(
        notifications_, seqNotification_, claim->claimantId, "claim", "认领已完成",
        "「" + item->title + "」已完成交接，感谢使用校园失物招领。", itemLink));
    pendingNotifications.push_back(PushNotification(
        notifications_, seqNotification_, item->userId, "item", "物品已完成交接",
        "「" + item->title + "」已核销取件码，本次失物招领完成。", itemLink));
    const std::string claimEvent = EventClaimUpdatedJson(*claim, *item);
    const std::string itemEvent = EventItemUpdatedJson(*item);
    out = *claim;
    SaveLocked(nullptr);
    EmitLocked(claimEvent, claim->claimantId);
    EmitLocked(claimEvent, item->userId);
    EmitLocked(itemEvent, 0);
    for (const Notification& notification : pendingNotifications) EmitNotificationLocked(notification);
    return true;
}

// ===========================================================================
// 通知
// ===========================================================================

void Store::AddNotification(int64_t userId, const std::string& type, const std::string& title,
                            const std::string& body, const std::string& link) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (userId <= 0) return;
    Notification notification = PushNotification(notifications_, seqNotification_, userId, type, title, body, link);
    SaveLocked(nullptr);
    EmitNotificationLocked(notification);
}

std::vector<Notification> Store::NotificationsFor(int64_t userId, bool unreadOnly, int limit) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Notification> result;
    for (const Notification& notification : notifications_) {
        if (notification.userId != userId) continue;
        if (unreadOnly && notification.read) continue;
        result.push_back(notification);
    }
    std::sort(result.begin(), result.end(), [](const Notification& a, const Notification& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    int take = limit > 0 ? limit : 50;
    if (take > 500) take = 500;
    if (result.size() > static_cast<std::size_t>(take)) {
        result.resize(static_cast<std::size_t>(take));
    }
    return result;
}

int Store::UnreadCount(int64_t userId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int count = 0;
    for (const Notification& notification : notifications_) {
        if (notification.userId == userId && !notification.read) ++count;
    }
    return count;
}

void Store::MarkNotificationsRead(int64_t userId, const std::vector<int64_t>& ids, bool markAll) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    bool changed = false;
    for (Notification& notification : notifications_) {
        if (notification.userId != userId || notification.read) continue;
        if (markAll || std::find(ids.begin(), ids.end(), notification.id) != ids.end()) {
            notification.read = true;
            changed = true;
        }
    }
    if (changed) SaveLocked(nullptr);
}

// ===========================================================================
// 举报 / 审计 / 统计
// ===========================================================================

int64_t Store::AddReport(int64_t reporterId, int64_t targetItemId, const std::string& reason) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Report report;
    report.id = ++seqReport_;
    report.reporterId = reporterId;
    report.targetType = "item";
    report.targetItemId = targetItemId;
    report.reason = reason;
    report.status = 0;
    report.handledBy = 0;
    report.handledAt = 0;
    report.createdAt = NowEpoch();
    reports_.push_back(report);
    SaveLocked(nullptr);
    return report.id;
}

std::vector<Report> Store::AllReports() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Report> result = reports_;
    std::sort(result.begin(), result.end(), [](const Report& a, const Report& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    return result;
}

bool Store::UpdateReportStatus(int64_t reportId, int status, int64_t handledBy, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    if (status < 0 || status > 2) {
        error = "举报状态无效";
        return false;
    }
    Report* report = FindById(reports_, reportId);
    if (report == nullptr) {
        error = "举报不存在";
        return false;
    }
    report->status = status;
    report->handledBy = handledBy;
    report->handledAt = NowEpoch();
    SaveLocked(nullptr);
    return true;
}

void Store::AddAudit(int64_t actorId, const std::string& action, const std::string& detail, const std::string& ip) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    AuditLog audit;
    audit.id = ++seqAudit_;
    audit.actorId = actorId;
    audit.action = action;
    audit.detail = detail;
    audit.ip = ip;
    audit.createdAt = NowEpoch();
    audits_.push_back(audit);
    if (audits_.size() > kMaxAuditLogs) {
        const std::size_t removeCount = audits_.size() - kMaxAuditLogs;
        audits_.erase(audits_.begin(), audits_.begin() + static_cast<std::ptrdiff_t>(removeCount));
    }
    SaveLocked(nullptr);
}

std::vector<AuditLog> Store::AuditLogs(int limit) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<AuditLog> result = audits_;
    std::sort(result.begin(), result.end(), [](const AuditLog& a, const AuditLog& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.id > b.id;
    });
    int take = limit > 0 ? limit : static_cast<int>(result.size());
    if (take > static_cast<int>(result.size())) take = static_cast<int>(result.size());
    result.resize(static_cast<std::size_t>(take));
    return result;
}

StoreStats Store::GetStats() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    StoreStats stats;
    stats.users = static_cast<int>(users_.size());
    stats.items = static_cast<int>(items_.size());
    for (const Item& item : items_) {
        if (item.status == kItemOpen) ++stats.openItems;
    }
    for (const Claim& claim : claims_) {
        if (claim.status == kClaimPending) ++stats.pendingClaims;
        if (claim.status == kClaimCompleted) ++stats.completedClaims;
    }
    for (const Report& report : reports_) {
        if (report.status == 0) ++stats.openReports;
    }
    return stats;
}

// ===========================================================================
// 过期扫描
// ===========================================================================

void Store::ExpireOverdue() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const int64_t now = NowEpoch();
    bool changed = false;
    std::vector<std::string> itemEvents;
    std::vector<std::pair<std::string, int64_t>> claimEvents;
    std::vector<Notification> pendingNotifications;

    // 1) 物品过期：status 0/1 且 expiresAt < now -> 4，并通知发布者；其进行中的认领申请一并关闭
    for (Item& item : items_) {
        if (item.status != kItemOpen && item.status != kItemClaimPending) continue;
        if (item.expiresAt <= 0 || item.expiresAt > now) continue;
        item.status = kItemExpired;
        item.updatedAt = now;
        changed = true;
        itemEvents.push_back(EventItemUpdatedJson(item));
        pendingNotifications.push_back(PushNotification(
            notifications_, seqNotification_, item.userId, "item", "物品已过期",
            "「" + item.title + "」已超过有效期，已自动标记为过期。", "#/item/" + item.publicId));
        for (Claim& claim : claims_) {
            if (claim.itemId != item.id) continue;
            if (claim.status != kClaimPending && claim.status != kClaimApproved) continue;
            claim.status = kClaimExpired;
            claim.reviewedAt = now;
            const std::string claimEvent = EventClaimUpdatedJson(claim, item);
            claimEvents.push_back(std::make_pair(claimEvent, claim.claimantId));
            claimEvents.push_back(std::make_pair(claimEvent, item.userId));
            pendingNotifications.push_back(PushNotification(
                notifications_, seqNotification_, claim.claimantId, "claim", "认领申请已关闭",
                "「" + item.title + "」已过期，你的认领申请已自动关闭。", "#/item/" + item.publicId));
        }
    }

    // 2) pending 认领超过 24h 未审核 -> 4，物品若为 1 则回到 0
    for (Claim& claim : claims_) {
        if (claim.status != kClaimPending) continue;
        if (claim.createdAt <= 0 || now - claim.createdAt < kClaimTimeoutSeconds) continue;
        claim.status = kClaimExpired;
        claim.reviewedAt = now;
        changed = true;
        Item* item = FindById(items_, claim.itemId);
        if (item == nullptr) continue;
        if (item->status == kItemClaimPending) {
            item->status = kItemOpen;
            item->updatedAt = now;
            itemEvents.push_back(EventItemUpdatedJson(*item));
        }
        const std::string eventJson = EventClaimUpdatedJson(claim, *item);
        claimEvents.push_back(std::make_pair(eventJson, claim.claimantId));
        claimEvents.push_back(std::make_pair(eventJson, item->userId));
        pendingNotifications.push_back(PushNotification(
            notifications_, seqNotification_, claim.claimantId, "claim", "认领申请已过期",
            "「" + item->title + "」的认领申请超过 24 小时未处理，已自动关闭。", "#/item/" + item->publicId));
    }

    // 3) approved 认取的取件码过期 -> 4，物品按自身是否过期置 0 或 4
    for (Claim& claim : claims_) {
        if (claim.status != kClaimApproved) continue;
        if (claim.pickupExpiresAt <= 0 || claim.pickupExpiresAt > now) continue;
        claim.status = kClaimExpired;
        changed = true;
        Item* item = FindById(items_, claim.itemId);
        if (item == nullptr) continue;
        const bool itemExpired = (item->expiresAt > 0 && item->expiresAt <= now);
        bool itemChanged = false;
        if (item->status == kItemClaimApproved) {
            const int nextStatus = itemExpired ? kItemExpired : kItemOpen;
            if (item->status != nextStatus) {
                item->status = nextStatus;
                item->updatedAt = now;
                itemChanged = true;
            }
        }
        if (itemChanged) itemEvents.push_back(EventItemUpdatedJson(*item));
        const std::string eventJson = EventClaimUpdatedJson(claim, *item);
        claimEvents.push_back(std::make_pair(eventJson, claim.claimantId));
        claimEvents.push_back(std::make_pair(eventJson, item->userId));
        pendingNotifications.push_back(PushNotification(
            notifications_, seqNotification_, claim.claimantId, "claim", "取件码已过期",
            "「" + item->title + "」的取件码已过期，认领申请已关闭。", "#/item/" + item->publicId));
        const std::string ownerBody = itemExpired
            ? ("「" + item->title + "」的取件码已过期，物品已过期。")
            : ("「" + item->title + "」的取件码已过期，物品已重新开放认领。");
        pendingNotifications.push_back(PushNotification(
            notifications_, seqNotification_, item->userId, "claim", "取件码已过期", ownerBody,
            "#/item/" + item->publicId));
    }

    if (!changed) return;
    SaveLocked(nullptr);
    for (const std::string& eventJson : itemEvents) EmitLocked(eventJson, 0);
    for (const auto& event : claimEvents) EmitLocked(event.first, event.second);
    for (const Notification& notification : pendingNotifications) EmitNotificationLocked(notification);
}

// ===========================================================================
// 持久化
// ===========================================================================

std::string Store::DumpJson() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    JsonValue root = MakeObj();
    root.Set("version", kDbVersion);

    JsonValue seq = MakeObj();
    seq.Set("user", seqUser_);
    seq.Set("item", seqItem_);
    seq.Set("claim", seqClaim_);
    seq.Set("notification", seqNotification_);
    seq.Set("report", seqReport_);
    seq.Set("audit", seqAudit_);
    root.Set("seq", std::move(seq));

    JsonValue categories = MakeArr();
    for (const Category& category : categories_) categories.Push(CategoryToJson(category));
    root.Set("categories", std::move(categories));

    JsonValue locations = MakeArr();
    for (const Location& location : locations_) locations.Push(LocationToJson(location));
    root.Set("locations", std::move(locations));

    JsonValue users = MakeArr();
    for (const User& user : users_) users.Push(UserToJson(user));
    root.Set("users", std::move(users));

    JsonValue items = MakeArr();
    for (const Item& item : items_) items.Push(ItemToJson(item));
    root.Set("items", std::move(items));

    JsonValue claims = MakeArr();
    for (const Claim& claim : claims_) claims.Push(ClaimToJson(claim));
    root.Set("claims", std::move(claims));

    JsonValue notifications = MakeArr();
    for (const Notification& notification : notifications_) notifications.Push(NotificationToJson(notification));
    root.Set("notifications", std::move(notifications));

    JsonValue reports = MakeArr();
    for (const Report& report : reports_) reports.Push(ReportToJson(report));
    root.Set("reports", std::move(reports));

    JsonValue audits = MakeArr();
    for (const AuditLog& audit : audits_) audits.Push(AuditToJson(audit));
    root.Set("audits", std::move(audits));

    JsonValue invites = MakeArr();
    for (const Invite& invite : invites_) invites.Push(InviteToJson(invite));
    root.Set("invites", std::move(invites));

    return root.Dump();
}

bool Store::LoadJson(const std::string& json, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    error.clear();
    JsonValue root;
    std::string parseError;
    if (!JsonValue::Parse(json, root, parseError)) {
        error = "数据库 JSON 解析失败：" + parseError;
        return false;
    }
    if (!root.IsObject()) {
        error = "数据库根节点必须是 JSON 对象";
        return false;
    }
    if (IntField(root, "version", kDbVersion) > kDbVersion) {
        error = "数据库版本过高，当前程序无法加载";
        return false;
    }

    categories_.clear();
    locations_.clear();
    users_.clear();
    items_.clear();
    claims_.clear();
    notifications_.clear();
    reports_.clear();
    audits_.clear();
    invites_.clear();
    seqUser_ = 0;
    seqItem_ = 0;
    seqClaim_ = 0;
    seqNotification_ = 0;
    seqReport_ = 0;
    seqAudit_ = 0;

    const JsonValue* categories = root.Find("categories");
    if (categories != nullptr && categories->IsArray()) {
        for (const JsonValue& element : categories->Items()) {
            if (element.IsObject()) categories_.push_back(CategoryFromJson(element));
        }
    }
    if (categories_.empty()) categories_ = DefaultCategories();

    const JsonValue* locations = root.Find("locations");
    if (locations != nullptr && locations->IsArray()) {
        for (const JsonValue& element : locations->Items()) {
            if (element.IsObject()) locations_.push_back(LocationFromJson(element));
        }
    }
    if (locations_.empty()) locations_ = DefaultLocations();

    LoadArrayField(root, "users", users_, UserFromJson);
    LoadArrayField(root, "items", items_, ItemFromJson);
    LoadArrayField(root, "claims", claims_, ClaimFromJson);
    LoadArrayField(root, "notifications", notifications_, NotificationFromJson);
    LoadArrayField(root, "reports", reports_, ReportFromJson);
    LoadArrayField(root, "audits", audits_, AuditFromJson);
    LoadArrayField(root, "invites", invites_, InviteFromJson);

    const JsonValue* seq = root.Find("seq");
    if (seq != nullptr && seq->IsObject()) {
        seqUser_ = IntField(*seq, "user", 0);
        seqItem_ = IntField(*seq, "item", 0);
        seqClaim_ = IntField(*seq, "claim", 0);
        seqNotification_ = IntField(*seq, "notification", 0);
        seqReport_ = IntField(*seq, "report", 0);
        seqAudit_ = IntField(*seq, "audit", 0);
    }
    // 容错：即使 seq 缺失/偏小，也保证不小于现有最大 ID，避免 ID 冲突
    seqUser_ = std::max(seqUser_, MaxId(users_));
    seqItem_ = std::max(seqItem_, MaxId(items_));
    seqClaim_ = std::max(seqClaim_, MaxId(claims_));
    seqNotification_ = std::max(seqNotification_, MaxId(notifications_));
    seqReport_ = std::max(seqReport_, MaxId(reports_));
    seqAudit_ = std::max(seqAudit_, MaxId(audits_));

    if (audits_.size() > kMaxAuditLogs) {
        const std::size_t removeCount = audits_.size() - kMaxAuditLogs;
        audits_.erase(audits_.begin(), audits_.begin() + static_cast<std::ptrdiff_t>(removeCount));
    }
    return true;
}

// ===========================================================================
// 私有实现
// ===========================================================================

bool Store::LoadLocked(const std::string& dataDir, std::string& error) {
    error.clear();
    if (dataDir.empty()) {
        error = "数据目录不能为空";
        return false;
    }
    const std::filesystem::path directory = std::filesystem::path(dataDir).lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = "无法创建数据目录：" + ec.message();
        return false;
    }
    std::filesystem::create_directories(directory / "uploads", ec);
    if (ec) {
        error = "无法创建上传目录：" + ec.message();
        return false;
    }
    dataDir_ = directory.string();

    const std::filesystem::path dbPath = directory / "db.json";
    ec.clear();
    const bool dbExists = std::filesystem::exists(dbPath, ec);
    if (ec) {
        error = "无法访问数据库文件：" + ec.message();
        return false;
    }
    if (dbExists) {
        std::ifstream input(dbPath, std::ios::binary);
        if (!input) {
            error = "无法读取数据库文件：" + dbPath.string();
            return false;
        }
        const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        if (content.empty()) {
            error = "数据库文件为空或已损坏：" + dbPath.string();
            return false;
        }
        std::string loadError;
        if (!LoadJson(content, loadError)) {
            error = "数据库文件加载失败：" + loadError;
            return false;
        }
        return true;
    }

    SeedLocked();
    std::string saveError;
    if (!SaveLocked(&saveError)) {
        error = "写入初始数据失败：" + saveError;
        return false;
    }
    return true;
}

bool Store::SaveLocked(std::string* errorOut) {
    if (errorOut != nullptr) errorOut->clear();
    if (dataDir_.empty()) {
        if (errorOut != nullptr) *errorOut = "数据目录未初始化";
        return false;
    }
    const std::filesystem::path directory(dataDir_);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        if (errorOut != nullptr) *errorOut = "无法创建数据目录：" + ec.message();
        return false;
    }
    const std::filesystem::path dbPath = directory / "db.json";
    const std::filesystem::path tempPath = directory / "db.json.tmp";
    const std::string json = DumpJson();
    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (errorOut != nullptr) *errorOut = "无法创建临时数据库文件";
            return false;
        }
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        output.flush();
        if (!output) {
            if (errorOut != nullptr) *errorOut = "写入临时数据库文件失败";
            return false;
        }
        output.close();
    }
    ec.clear();
    std::filesystem::rename(tempPath, dbPath, ec);
    if (ec) {
        // 某些情况下（目标被占用、跨卷、只读标记等）rename 无法覆盖，退化为先删后改名。
        std::error_code removeError;
        std::filesystem::remove(dbPath, removeError);
        ec.clear();
        std::filesystem::rename(tempPath, dbPath, ec);
        if (ec) {
            std::error_code cleanupError;
            std::filesystem::remove(tempPath, cleanupError);
            if (errorOut != nullptr) *errorOut = "替换数据库文件失败：" + ec.message();
            return false;
        }
    }
    return true;
}

void Store::SeedLocked() {
    categories_ = DefaultCategories();
    locations_ = DefaultLocations();
    users_.clear();
    items_.clear();
    claims_.clear();
    notifications_.clear();
    reports_.clear();
    audits_.clear();
    invites_.clear();
    seqUser_ = 0;
    seqItem_ = 0;
    seqClaim_ = 0;
    seqNotification_ = 0;
    seqReport_ = 0;
    seqAudit_ = 0;

    const int64_t now = NowEpoch();
    User admin;
    admin.id = ++seqUser_;
    admin.studentNo = "admin";
    admin.name = "系统管理员";
    admin.college = "信息中心";
    // 首次初始化生成随机管理员密码，明文只在内存中保留到控制台打印一次，不落库、不写入源码。
    const std::string bootstrapPassword = RandomHex(8);
    admin.passwordHash = Auth::Instance().HashPassword(bootstrapPassword);
    admin.role = "admin";
    admin.status = 1;
    admin.createdAt = now;
    admin.lastLoginAt = 0;
    users_.push_back(admin);
    initialAdminPassword_ = bootstrapPassword;
    // 邀请码不再预置：管理员登录后台后按需创建，完整码仅在创建时显示一次。
}

void Store::EmitLocked(const std::string& eventJson, int64_t targetUserId) {
    if (eventSink_) {
        eventSink_(eventJson, targetUserId);
    }
}

void Store::EmitNotificationLocked(const Notification& notification) {
    int unreadCount = 0;
    for (const Notification& item : notifications_) {
        if (item.userId == notification.userId && !item.read) ++unreadCount;
    }
    JsonValue event = MakeObj();
    event.Set("type", "notification");
    event.Set("notification", NotificationViewToJson(notification));
    event.Set("unreadCount", unreadCount);
    EmitLocked(event.Dump(), notification.userId);
}

std::string Store::StatusText(int status) const {
    return ItemStatusText(status);
}

} // namespace lf
