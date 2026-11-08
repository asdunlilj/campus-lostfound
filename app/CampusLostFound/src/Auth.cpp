#include "Auth.h"
#include "Util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace lf {

namespace {

const int kIterations = 60000;
const int64_t kSessionAbsoluteSeconds = 12 * 60 * 60;
const int64_t kSessionIdleSeconds = 30 * 60;

bool DeriveKey(const std::string& password, const std::vector<unsigned char>& salt, uint64_t iterations,
               unsigned char* out, size_t outLen) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) == 0) {
        ok = BCryptDeriveKeyPBKDF2(
                 alg,
                 reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
                 static_cast<ULONG>(password.size()),
                 const_cast<PUCHAR>(salt.data()),
                 static_cast<ULONG>(salt.size()),
                 iterations,
                 out, static_cast<ULONG>(outLen), 0) == 0;
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return ok;
}

std::vector<std::string> SplitDollar(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == '$') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

} // namespace

Auth& Auth::Instance() {
    static Auth instance;
    return instance;
}

std::string Auth::HashPassword(const std::string& password) const {
    std::vector<unsigned char> salt(16);
    if (BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        // 极端情况下用随机十六进制填充
        std::string fallback = RandomHex(16);
        for (size_t i = 0; i < salt.size() && i < fallback.size(); ++i) {
            salt[i] = static_cast<unsigned char>(fallback[i]);
        }
    }
    unsigned char hash[32] = {0};
    if (!DeriveKey(password, salt, kIterations, hash, sizeof(hash))) {
        return std::string();
    }
    std::string saltStr(reinterpret_cast<const char*>(salt.data()), salt.size());
    std::string hashStr(reinterpret_cast<const char*>(hash), sizeof(hash));
    return "pbkdf2-sha256$" + std::to_string(kIterations) + "$" + Base64Encode(saltStr) + "$" +
           Base64Encode(hashStr);
}

bool Auth::VerifyPassword(const std::string& password, const std::string& stored) const {
    std::vector<std::string> parts = SplitDollar(stored);
    if (parts.size() != 4 || parts[0] != "pbkdf2-sha256") return false;
    int iterations = 0;
    if (!ParseInt(parts[1], iterations) || iterations <= 0) return false;
    std::string saltStr = Base64Decode(parts[2]);
    std::string expected = Base64Decode(parts[3]);
    if (expected.size() != 32) return false;
    std::vector<unsigned char> salt(saltStr.begin(), saltStr.end());
    unsigned char hash[32] = {0};
    if (!DeriveKey(password, salt, static_cast<uint64_t>(iterations), hash, sizeof(hash))) return false;
    std::string actual(reinterpret_cast<const char*>(hash), sizeof(hash));
    return ConstantTimeEquals(actual, expected);
}

std::string Auth::CreateSession(int64_t userId) {
    std::string token = RandomHex(32);
    int64_t now = NowEpoch();
    Session session;
    session.userId = userId;
    session.createdAt = now;
    session.lastSeenAt = now;
    session.expiresAt = now + kSessionAbsoluteSeconds;
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = session;
    return token;
}

std::optional<int64_t> Auth::ValidateSession(const std::string& token, bool touch) {
    if (token.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return std::nullopt;
    int64_t now = NowEpoch();
    if (it->second.expiresAt <= now || it->second.lastSeenAt + kSessionIdleSeconds <= now) {
        sessions_.erase(it);
        return std::nullopt;
    }
    if (touch) it->second.lastSeenAt = now;
    return it->second.userId;
}

void Auth::RevokeSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

void Auth::RevokeAllForUser(int64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.userId == userId) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void Auth::SweepExpired() {
    int64_t now = NowEpoch();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expiresAt <= now || it->second.lastSeenAt + kSessionIdleSeconds <= now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = attempts_.begin(); it != attempts_.end();) {
        if (now - it->second.windowStart > 3600) {
            it = attempts_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Auth::AllowAttempt(const std::string& key, int maxCount, int windowSeconds) {
    int64_t now = NowEpoch();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attempts_.find(key);
    if (it == attempts_.end()) {
        attempts_[key] = RateEntry{1, now};
        return true;
    }
    if (now - it->second.windowStart >= windowSeconds) {
        it->second.count = 1;
        it->second.windowStart = now;
        return true;
    }
    if (it->second.count >= maxCount) return false;
    ++it->second.count;
    return true;
}

void Auth::ResetAttempts(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    attempts_.erase(key);
}

} // namespace lf