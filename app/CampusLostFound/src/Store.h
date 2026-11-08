#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lf {

struct Category {
    int id = 0;
    std::string name;
    int sort = 0;
};

struct Location {
    int id = 0;
    std::string name;
    std::string building;
    int sort = 0;
};

struct User {
    int64_t id = 0;
    std::string studentNo;
    std::string name;
    std::string college;
    std::string passwordHash;
    std::string role;   // "student" | "admin"
    int status = 1;     // 1 正常 / 0 禁用
    int64_t createdAt = 0;
    int64_t lastLoginAt = 0;
};

struct ItemImage {
    std::string id;
    std::string fileName;
    int sort = 0;
};

struct Item {
    int64_t id = 0;
    std::string publicId;
    int type = 1;                  // 0 寻物(lost) / 1 招领(found)
    int64_t userId = 0;
    std::string title;
    std::string description;
    int categoryId = 0;
    int locationId = 0;
    std::string eventTime;         // 例如 2026-11-08 14:30（用户填写）
    int status = 0;                // 0 open, 1 claim_pending, 2 claim_approved, 3 completed, 4 expired, 5 removed
    std::string verifyQuestion;    // 认领核验问题（可空）
    int64_t expiresAt = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    int viewCount = 0;
    std::vector<ItemImage> images;
    std::string removedReason;
};

struct Claim {
    int64_t id = 0;
    int64_t itemId = 0;
    int64_t claimantId = 0;
    int status = 0;                // 0 pending, 1 approved, 2 rejected, 3 cancelled, 4 expired, 5 completed
    std::string answer;
    std::string message;
    std::string pickupCode;        // 6 位数字；批准后生成
    int64_t pickupExpiresAt = 0;
    int64_t createdAt = 0;
    int64_t reviewedAt = 0;
    int64_t completedAt = 0;
};

struct Notification {
    int64_t id = 0;
    int64_t userId = 0;
    std::string type;
    std::string title;
    std::string body;
    std::string link;              // 前端哈希路由，例如 #/item/<publicId>
    bool read = false;
    int64_t createdAt = 0;
};

struct Report {
    int64_t id = 0;
    int64_t reporterId = 0;
    std::string targetType;        // "item"
    int64_t targetItemId = 0;
    std::string reason;
    int status = 0;                // 0 open, 1 resolved, 2 dismissed
    int64_t handledBy = 0;
    int64_t handledAt = 0;
    int64_t createdAt = 0;
};

struct AuditLog {
    int64_t id = 0;
    int64_t actorId = 0;
    std::string action;
    std::string detail;
    std::string ip;
    int64_t createdAt = 0;
};

struct Invite {
    std::string code;
    int maxUses = 0;
    int uses = 0;
    int64_t expiresAt = 0;
    bool disabled = false;
    std::string createdBy;
    int64_t createdAt = 0;
};

struct StoreStats {
    int users = 0;
    int items = 0;
    int openItems = 0;
    int pendingClaims = 0;
    int completedClaims = 0;
    int openReports = 0;
};

// 数据仓库（单例）。线程安全；每次变更自动落盘 db.json 并广播事件。
// 事件通过 SetEventSink 注入，eventJson 是可直接推给浏览器的 JSON 字符串，
// targetUserId = 0 表示广播给所有在线用户，否则只发给该用户。
class Store {
public:
    static Store& Instance();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // dataDir 不存在则创建；无 db.json 时写入种子数据（管理员 + 邀请码）
    bool Load(const std::string& dataDir, std::string& error);
    bool Save(std::string* errorOut = nullptr);
    const std::string& DataDir() const;
    void SetEventSink(std::function<void(const std::string& eventJson, int64_t targetUserId)> sink);

    const std::vector<Category>& Categories() const;
    const std::vector<Location>& Locations() const;

    // ---- 用户 ----
    std::optional<User> FindUserById(int64_t id) const;
    std::optional<User> FindUserByStudentNo(const std::string& studentNo) const;
    std::vector<User> AllUsers() const;
    int64_t CreateUser(const std::string& studentNo, const std::string& name, const std::string& college,
                       const std::string& passwordHash, const std::string& role, std::string& error);
    bool UpdateLastLogin(int64_t userId);
    bool ChangePassword(int64_t userId, const std::string& newHash);

    // ---- 邀请码 ----
    std::vector<Invite> AllInvites() const;
    std::optional<Invite> FindInvite(const std::string& code) const;
    Invite CreateInvite(int maxUses, int validDays, const std::string& createdBy);
    bool ConsumeInvite(const std::string& code, std::string& error);

    // ---- 物品 ----
    // statusFilter: "" 或 "active"(0,1,2) / "open" / "claim_pending" / "claim_approved"
    //               / "completed" / "expired" / "removed" / "all"
    std::vector<Item> QueryItems(int type, int categoryId, int locationId, const std::string& statusFilter,
                                 const std::string& keyword, int offset, int limit, int& total) const;
    std::optional<Item> FindItemById(int64_t id) const;
    std::optional<Item> FindItemByPublicId(const std::string& publicId) const;
    bool CreateItem(Item& item, std::string& error);
    bool UpdateItem(int64_t itemId, const std::string& title, const std::string& description,
                    int categoryId, int locationId, const std::string& eventTime,
                    const std::string& verifyQuestion, std::string& error);
    bool SetItemStatus(int64_t itemId, int status, const std::string& reason, std::string& error);
    void IncrementView(int64_t itemId);
    bool AddItemImage(int64_t itemId, const std::string& fileName, std::string& imageId, std::string& error);
    bool RemoveItemImage(int64_t itemId, const std::string& imageId, std::string& error);

    // ---- 认领 ----
    std::optional<Claim> FindClaimById(int64_t id) const;
    std::vector<Claim> ClaimsForItem(int64_t itemId) const;
    std::vector<Claim> ClaimsByUser(int64_t userId) const;
    int CountClaimsForItem(int64_t itemId) const;
    bool CreateClaim(Claim& claim, std::string& error);
    bool ApproveClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error);
    bool RejectClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error);
    bool CancelClaim(int64_t claimId, int64_t actorId, Claim& out, std::string& error);
    bool VerifyClaim(int64_t claimId, int64_t actorId, const std::string& code, Claim& out, std::string& error);

    // ---- 通知 ----
    void AddNotification(int64_t userId, const std::string& type, const std::string& title,
                         const std::string& body, const std::string& link);
    std::vector<Notification> NotificationsFor(int64_t userId, bool unreadOnly, int limit) const;
    int UnreadCount(int64_t userId) const;
    void MarkNotificationsRead(int64_t userId, const std::vector<int64_t>& ids, bool markAll);

    // ---- 举报 / 审计 / 统计 ----
    int64_t AddReport(int64_t reporterId, int64_t targetItemId, const std::string& reason);
    std::vector<Report> AllReports() const;
    bool UpdateReportStatus(int64_t reportId, int status, int64_t handledBy, std::string& error);

    void AddAudit(int64_t actorId, const std::string& action, const std::string& detail, const std::string& ip);
    std::vector<AuditLog> AuditLogs(int limit) const;

    StoreStats GetStats() const;

    // ---- 维护 ----
    void ExpireOverdue();

    // ---- 持久化（自测/工具用） ----
    std::string DumpJson() const;
    bool LoadJson(const std::string& json, std::string& error);

private:
    Store() = default;

    bool LoadLocked(const std::string& dataDir, std::string& error);
    bool SaveLocked(std::string* errorOut);
    void SeedLocked();
    void EmitLocked(const std::string& eventJson, int64_t targetUserId);
    void EmitNotificationLocked(const Notification& n);
    std::string StatusText(int status) const;

    mutable std::recursive_mutex mutex_;
    std::string dataDir_;
    std::function<void(const std::string&, int64_t)> eventSink_;

    std::vector<Category> categories_;
    std::vector<Location> locations_;

    std::vector<User> users_;
    std::vector<Item> items_;
    std::vector<Claim> claims_;
    std::vector<Notification> notifications_;
    std::vector<Report> reports_;
    std::vector<AuditLog> audits_;
    std::vector<Invite> invites_;

    int64_t seqUser_ = 0;
    int64_t seqItem_ = 0;
    int64_t seqClaim_ = 0;
    int64_t seqNotification_ = 0;
    int64_t seqReport_ = 0;
    int64_t seqAudit_ = 0;
};

} // namespace lf