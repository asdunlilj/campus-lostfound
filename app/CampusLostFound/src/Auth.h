#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace lf {

// 认证：PBKDF2-HMAC-SHA256 密码哈希 + 服务端会话（内存）+ 简单限流。
class Auth {
public:
    static Auth& Instance();

    Auth(const Auth&) = delete;
    Auth& operator=(const Auth&) = delete;

    // 返回格式：pbkdf2-sha256$<iterations>$<saltBase64>$<hashBase64>
    std::string HashPassword(const std::string& password) const;
    bool VerifyPassword(const std::string& password, const std::string& stored) const;

    std::string CreateSession(int64_t userId);
    std::optional<int64_t> ValidateSession(const std::string& token, bool touch);
    void RevokeSession(const std::string& token);
    void RevokeAllForUser(int64_t userId);
    void SweepExpired();

    // 限流：key 在 windowSeconds 内最多 maxCount 次（调用即计数）
    bool AllowAttempt(const std::string& key, int maxCount, int windowSeconds);
    void ResetAttempts(const std::string& key);

private:
    struct Session {
        int64_t userId = 0;
        int64_t createdAt = 0;
        int64_t lastSeenAt = 0;
        int64_t expiresAt = 0;
    };
    struct RateEntry {
        int count = 0;
        int64_t windowStart = 0;
    };

    Auth() = default;

    std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
    std::unordered_map<std::string, RateEntry> attempts_;
};

} // namespace lf