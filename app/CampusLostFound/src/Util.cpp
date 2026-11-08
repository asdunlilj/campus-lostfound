#include "Util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace lf {

std::string Trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string ToLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

std::string Join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

std::string ReplaceAll(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::string out;
    out.reserve(s.size());
    size_t pos = 0;
    for (;;) {
        size_t hit = s.find(from, pos);
        if (hit == std::string::npos) {
            out.append(s, pos, std::string::npos);
            break;
        }
        out.append(s, pos, hit - pos);
        out += to;
        pos = hit + from.size();
    }
    return out;
}

static int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string UrlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            int hi = HexDigit(s[i + 1]);
            int lo = HexDigit(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(s[i]);
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::string HtmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

std::string RandomHex(int bytes) {
    if (bytes <= 0) return std::string();
    std::vector<unsigned char> buf(static_cast<size_t>(bytes));
    NTSTATUS st = BCryptGenRandom(nullptr, buf.data(), static_cast<ULONG>(buf.size()),
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (st != 0) {
        // 理论上不会失败；兜底用时间+rand，避免返回空
        std::mt19937_64 rng(static_cast<unsigned long long>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        for (auto& b : buf) b = static_cast<unsigned char>(rng() & 0xFF);
    }
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(buf.size() * 2);
    for (unsigned char b : buf) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

std::string NewPublicId() {
    return RandomHex(12);
}

bool ConstantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::string& bytes) {
    std::string out;
    size_t i = 0;
    while (i + 2 < bytes.size()) {
        unsigned v = (static_cast<unsigned char>(bytes[i]) << 16) |
                     (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                     static_cast<unsigned char>(bytes[i + 2]);
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]);
        out.push_back(kB64[v & 63]);
        i += 3;
    }
    if (i + 1 == bytes.size()) {
        unsigned v = static_cast<unsigned char>(bytes[i]) << 16;
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out += "==";
    } else if (i + 2 == bytes.size()) {
        unsigned v = (static_cast<unsigned char>(bytes[i]) << 16) |
                     (static_cast<unsigned char>(bytes[i + 1]) << 8);
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::string Base64Decode(const std::string& text) {
    int table[256];
    for (int i = 0; i < 256; ++i) table[i] = -1;
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(kB64[i])] = i;
    std::string out;
    unsigned buf = 0;
    int bits = 0;
    for (unsigned char c : text) {
        if (c == '=') break;
        int v = table[c];
        if (v < 0) continue;
        buf = (buf << 6) | static_cast<unsigned>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::string Sha256Hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string result;
    unsigned char digest[32];
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return result;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(data.data());
        size_t left = data.size();
        size_t offset = 0;
        while (left > 0) {
            ULONG chunk = static_cast<ULONG>(left > 0x40000000 ? 0x40000000 : left);
            BCryptHashData(hash, const_cast<PUCHAR>(p + offset), chunk, 0);
            offset += chunk;
            left -= chunk;
        }
        if (BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            static const char* hex = "0123456789abcdef";
            result.reserve(64);
            for (unsigned char b : digest) {
                result.push_back(hex[b >> 4]);
                result.push_back(hex[b & 0x0F]);
            }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

int64_t NowEpoch() {
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count());
}

std::string FormatIso8601(int64_t epochSeconds) {
    if (epochSeconds <= 0) return std::string();
    std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
    gmtime_s(&tm, &t);
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::string FormatLocalTime(int64_t epochSeconds) {
    if (epochSeconds <= 0) return std::string();
    std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return std::string(buf);
}

std::string FormatTimeAgo(int64_t epochSeconds) {
    if (epochSeconds <= 0) return std::string();
    int64_t diff = NowEpoch() - epochSeconds;
    if (diff < 0) diff = 0;
    if (diff < 60) return "刚刚";
    if (diff < 3600) return std::to_string(diff / 60) + " 分钟前";
    if (diff < 86400) return std::to_string(diff / 3600) + " 小时前";
    if (diff < 86400 * 30) return std::to_string(diff / 86400) + " 天前";
    return FormatLocalTime(epochSeconds).substr(0, 10);
}

bool ParseInt64(const std::string& s, int64_t& out) {
    std::string t = Trim(s);
    if (t.empty()) return false;
    errno = 0;
    char* end = nullptr;
    long long v = std::strtoll(t.c_str(), &end, 10);
    if (errno != 0 || end == t.c_str() || *end != '\0') return false;
    out = static_cast<int64_t>(v);
    return true;
}

bool ParseInt(const std::string& s, int& out) {
    int64_t v = 0;
    if (!ParseInt64(s, v)) return false;
    out = static_cast<int>(v);
    return true;
}

std::string ToString(int64_t v) {
    return std::to_string(v);
}

static size_t Utf8CharLen(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string MaskName(const std::string& name) {
    if (name.empty()) return std::string("匿名");
    size_t len = Utf8CharLen(static_cast<unsigned char>(name[0]));
    if (len > name.size()) len = name.size();
    return name.substr(0, len) + "*";
}

std::string MaskPhone(const std::string& phone) {
    if (phone.size() < 7) return std::string("***");
    return phone.substr(0, 3) + "****" + phone.substr(phone.size() - 4);
}

std::string MaskStudentNo(const std::string& no) {
    if (no.size() <= 4) return std::string("****");
    return no.substr(0, 4) + "****";
}

std::string SanitizeAscii(const std::string& s) {
    std::string out;
    for (char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-') {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace lf