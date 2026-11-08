#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lf {

std::string Trim(const std::string& s);
bool StartsWith(const std::string& s, const std::string& prefix);
bool EndsWith(const std::string& s, const std::string& suffix);
std::string ToLowerAscii(const std::string& s);
std::vector<std::string> Split(const std::string& s, char delim);
std::string Join(const std::vector<std::string>& parts, const std::string& sep);
std::string ReplaceAll(const std::string& s, const std::string& from, const std::string& to);
std::string UrlDecode(const std::string& s);
std::string UrlEncode(const std::string& s);
std::string HtmlEscape(const std::string& s);

// 加密随机数（BCryptGenRandom），返回 2*bytes 个小写十六进制字符
std::string RandomHex(int bytes);
// 24 位十六进制公开 ID（用于请求 URL，避免自增 ID 被遍历）
std::string NewPublicId();
bool ConstantTimeEquals(const std::string& a, const std::string& b);

std::string Base64Encode(const std::string& bytes);
std::string Base64Decode(const std::string& text);
// SHA-256 十六进制摘要（BCrypt），用于会话令牌/文件名等短摘要
std::string Sha256Hex(const std::string& data);

int64_t NowEpoch();
std::string FormatIso8601(int64_t epochSeconds);   // UTC: 2026-11-08T10:30:00Z
std::string FormatLocalTime(int64_t epochSeconds); // 2026-11-08 18:30:00
std::string FormatTimeAgo(int64_t epochSeconds);

bool ParseInt64(const std::string& s, int64_t& out);
bool ParseInt(const std::string& s, int& out);
std::string ToString(int64_t v);

std::string MaskName(const std::string& name);          // 张三 -> 张*
std::string MaskPhone(const std::string& phone);        // 13812341234 -> 138****1234
std::string MaskStudentNo(const std::string& no);       // 20230001 -> 2023****
std::string SanitizeAscii(const std::string& s);        // 只保留 [A-Za-z0-9._-]

} // namespace lf