#include "Http.h"
#include "Util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace lf {

namespace {

const size_t kMaxHeaderBytes = 64 * 1024;
const size_t kMaxBodyBytes = 16 * 1024 * 1024;

std::string ToLowerStr(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

const char* StatusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "OK";
    }
}

std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : path) {
        if (c == '/') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool IsParamSegment(const std::string& seg) {
    return seg.size() >= 2 && seg.front() == '{' && seg.back() == '}';
}

bool MatchSegments(const std::vector<std::string>& pattern, const std::vector<std::string>& actual,
                   std::map<std::string, std::string>& params) {
    if (pattern.size() != actual.size()) return false;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (IsParamSegment(pattern[i])) {
            params[pattern[i].substr(1, pattern[i].size() - 2)] = UrlDecode(actual[i]);
        } else if (pattern[i] != actual[i]) {
            return false;
        }
    }
    return true;
}

// 解析 multipart/form-data
void ParseMultipart(const std::string& contentType, const std::string& body,
                    std::vector<HttpRequest::Upload>& out) {
    std::string boundary;
    size_t pos = contentType.find("boundary=");
    if (pos == std::string::npos) return;
    boundary = contentType.substr(pos + 9);
    if (!boundary.empty() && boundary.front() == '"') {
        size_t end = boundary.find('"', 1);
        if (end != std::string::npos) boundary = boundary.substr(1, end - 1);
    }
    if (boundary.empty()) return;

    const std::string delimiter = "--" + boundary;
    const std::string crlf = "\r\n";

    size_t cursor = 0;
    while (true) {
        size_t start = body.find(delimiter, cursor);
        if (start == std::string::npos) break;
        size_t after = start + delimiter.size();
        if (after + 2 <= body.size() && body.compare(after, 2, "--") == 0) break;
        if (after + 2 <= body.size() && body.compare(after, 2, crlf) == 0) after += 2;

        size_t headEnd = body.find("\r\n\r\n", after);
        if (headEnd == std::string::npos) break;
        std::string partHeaders = body.substr(after, headEnd - after);

        size_t next = body.find(crlf + delimiter, headEnd + 4);
        if (next == std::string::npos) break;
        std::string content = body.substr(headEnd + 4, next - (headEnd + 4));

        HttpRequest::Upload upload;
        for (const std::string& line : Split(partHeaders, '\n')) {
            std::string l = Trim(line);
            if (l.empty()) continue;
            size_t colon = l.find(':');
            if (colon == std::string::npos) continue;
            std::string name = ToLowerStr(Trim(l.substr(0, colon)));
            std::string value = Trim(l.substr(colon + 1));
            if (name == "content-type") {
                upload.contentType = value;
            } else if (name == "content-disposition") {
                size_t np = value.find("name=");
                if (np != std::string::npos) {
                    size_t begin = np + 5;
                    if (begin < value.size() && value[begin] == '"') {
                        size_t end = value.find('"', begin + 1);
                        if (end != std::string::npos) upload.fieldName = value.substr(begin + 1, end - begin - 1);
                    } else {
                        size_t end = value.find(';', begin);
                        upload.fieldName = value.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
                    }
                }
                size_t fp = value.find("filename=");
                if (fp != std::string::npos) {
                    size_t begin = fp + 9;
                    if (begin < value.size() && value[begin] == '"') {
                        size_t end = value.find('"', begin + 1);
                        if (end != std::string::npos) upload.fileName = value.substr(begin + 1, end - begin - 1);
                    } else {
                        size_t end = value.find(';', begin);
                        upload.fileName = value.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
                    }
                }
            }
        }
        upload.data = std::move(content);
        out.push_back(std::move(upload));
        cursor = next + crlf.size();
    }
}

} // namespace

bool SocketSendAll(intptr_t socketHandle, const std::string& data) {
    SOCKET s = static_cast<SOCKET>(socketHandle);
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string HttpRequest::QueryValue(const std::string& key, const std::string& def) const {
    auto it = query.find(key);
    return it == query.end() ? def : it->second;
}

std::string HttpRequest::HeaderValue(const std::string& key, const std::string& def) const {
    auto it = headers.find(ToLowerStr(key));
    return it == headers.end() ? def : it->second;
}

std::string HttpRequest::CookieValue(const std::string& key) const {
    std::string cookie = HeaderValue("cookie");
    for (const std::string& part : Split(cookie, ';')) {
        std::string item = Trim(part);
        size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        if (Trim(item.substr(0, eq)) == key) return item.substr(eq + 1);
    }
    return std::string();
}

std::string HttpRequest::PathValue(const std::string& key, const std::string& def) const {
    auto it = pathParams.find(key);
    return it == pathParams.end() ? def : it->second;
}

bool HttpRequest::IsMultipart() const {
    return ToLowerStr(HeaderValue("content-type")).find("multipart/form-data") != std::string::npos;
}

HttpResponse HttpResponse::Json(const std::string& body, int status) {
    HttpResponse r;
    r.status = status;
    r.contentType = "application/json; charset=utf-8";
    r.body = body;
    return r;
}

HttpResponse HttpResponse::Text(const std::string& body, int status) {
    HttpResponse r;
    r.status = status;
    r.contentType = "text/plain; charset=utf-8";
    r.body = body;
    return r;
}

HttpResponse HttpResponse::Html(const std::string& body, int status) {
    HttpResponse r;
    r.status = status;
    r.contentType = "text/html; charset=utf-8";
    r.body = body;
    return r;
}

HttpResponse HttpResponse::Binary(const std::string& body, const std::string& contentType) {
    HttpResponse r;
    r.status = 200;
    r.contentType = contentType;
    r.body = body;
    return r;
}

void HttpResponse::AddCookie(const std::string& name, const std::string& value, int maxAgeSeconds,
                             bool httpOnly, bool secure) {
    std::string c = name + "=" + value + "; Path=/; SameSite=Lax";
    if (httpOnly) c += "; HttpOnly";
    if (secure) c += "; Secure";
    if (maxAgeSeconds > 0) c += "; Max-Age=" + std::to_string(maxAgeSeconds);
    if (maxAgeSeconds < 0) c += "; Max-Age=0";
    setCookies.push_back(c);
}

void HttpResponse::ClearCookie(const std::string& name) {
    setCookies.push_back(name + "=; Path=/; Max-Age=0; SameSite=Lax; HttpOnly");
}

struct HttpServer::Impl {
    struct Route {
        std::string method;
        std::vector<std::string> segments;
        HttpHandler handler;
    };
    struct StreamRoute {
        std::string method;
        std::vector<std::string> segments;
        HttpStreamHandler handler;
    };

    SOCKET listenSocket = INVALID_SOCKET;
    std::atomic<bool> running{false};
    std::thread acceptThread;
    std::vector<Route> routes;
    std::vector<StreamRoute> streamRoutes;
    HttpHandler fallback;
    bool wsaStarted = false;

    void HandleConnection(SOCKET client, const std::string& remoteIp) {
        // 读请求头
        std::string buffer;
        char temp[8192];
        size_t headerEnd = std::string::npos;
        while ((headerEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
            if (buffer.size() > kMaxHeaderBytes) {
                CloseWith(client, 413);
                return;
            }
            int n = recv(client, temp, sizeof(temp), 0);
            if (n <= 0) {
                closesocket(client);
                return;
            }
            buffer.append(temp, static_cast<size_t>(n));
        }

        std::string headerPart = buffer.substr(0, headerEnd);
        std::string body = buffer.substr(headerEnd + 4);

        HttpRequest req;
        req.remoteIp = remoteIp;

        std::vector<std::string> lines = Split(headerPart, '\n');
        if (lines.empty()) {
            CloseWith(client, 400);
            return;
        }
        std::string requestLine = Trim(lines[0]);
        {
            std::vector<std::string> parts = Split(requestLine, ' ');
            if (parts.size() < 2) {
                CloseWith(client, 400);
                return;
            }
            req.method = parts[0];
            std::string target = parts[1];
            size_t qm = target.find('?');
            if (qm == std::string::npos) {
                req.path = target;
            } else {
                req.path = target.substr(0, qm);
                req.queryString = target.substr(qm + 1);
            }
            if (req.path.empty()) req.path = "/";
        }
        for (size_t i = 1; i < lines.size(); ++i) {
            std::string line = lines[i];
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string name = ToLowerStr(Trim(line.substr(0, colon)));
            std::string value = Trim(line.substr(colon + 1));
            req.headers[name] = value;
        }
        // 查询参数
        if (!req.queryString.empty()) {
            for (const std::string& pair : Split(req.queryString, '&')) {
                if (pair.empty()) continue;
                size_t eq = pair.find('=');
                if (eq == std::string::npos) {
                    req.query[UrlDecode(pair)] = "";
                } else {
                    req.query[UrlDecode(pair.substr(0, eq))] = UrlDecode(pair.substr(eq + 1));
                }
            }
        }
        // 请求体
        size_t contentLength = 0;
        {
            std::string cl = req.HeaderValue("content-length");
            if (!cl.empty()) {
                int64_t parsed = 0;
                if (ParseInt64(cl, parsed) && parsed >= 0) contentLength = static_cast<size_t>(parsed);
            }
        }
        if (contentLength > kMaxBodyBytes) {
            CloseWith(client, 413);
            return;
        }
        while (body.size() < contentLength) {
            int n = recv(client, temp, sizeof(temp), 0);
            if (n <= 0) {
                closesocket(client);
                return;
            }
            body.append(temp, static_cast<size_t>(n));
        }
        if (body.size() > contentLength) body.resize(contentLength);
        req.body = std::move(body);

        if (req.IsMultipart()) ParseMultipart(req.HeaderValue("content-type"), req.body, req.uploads);

        // 路由
        std::vector<std::string> segments = SplitPath(req.path);
        for (const Route& route : routes) {
            if (route.method != req.method) continue;
            std::map<std::string, std::string> params;
            if (!MatchSegments(route.segments, segments, params)) continue;
            req.pathParams = std::move(params);
            HttpResponse response;
            try {
                response = route.handler(req);
            } catch (const std::exception&) {
                response = HttpResponse::Json("{\"ok\":false,\"error\":{\"code\":\"INTERNAL\",\"message\":\"服务器内部错误\"}}", 500);
            } catch (...) {
                response = HttpResponse::Json("{\"ok\":false,\"error\":{\"code\":\"INTERNAL\",\"message\":\"服务器内部错误\"}}", 500);
            }
            SendResponse(client, response);
            closesocket(client);
            return;
        }
        for (const StreamRoute& route : streamRoutes) {
            if (route.method != req.method) continue;
            std::map<std::string, std::string> params;
            if (!MatchSegments(route.segments, segments, params)) continue;
            req.pathParams = std::move(params);
            try {
                route.handler(req, static_cast<intptr_t>(client));
            } catch (...) {
            }
            closesocket(client);
            return;
        }
        if (fallback) {
            HttpResponse response;
            try {
                response = fallback(req);
            } catch (...) {
                response = HttpResponse::Json("{\"ok\":false,\"error\":{\"code\":\"INTERNAL\",\"message\":\"服务器内部错误\"}}", 500);
            }
            SendResponse(client, response);
        } else {
            Response404(client);
        }
        closesocket(client);
    }

    void SendResponse(SOCKET client, const HttpResponse& response) {
        std::string out = "HTTP/1.1 " + std::to_string(response.status) + " " + StatusText(response.status) + "\r\n";
        out += "Content-Type: " + response.contentType + "\r\n";
        out += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
        out += "Connection: close\r\n";
        out += "X-Content-Type-Options: nosniff\r\n";
        out += "Referrer-Policy: no-referrer\r\n";
        out += "X-Frame-Options: DENY\r\n";
        out += "Content-Security-Policy: default-src 'self'; img-src 'self' data: blob:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'\r\n";
        bool hasCacheControl = false;
        for (const auto& header : response.headers) {
            if (ToLowerStr(header.first) == "cache-control") {
                hasCacheControl = true;
                break;
            }
        }
        if (!hasCacheControl) out += "Cache-Control: no-store\r\n";
        for (const auto& header : response.headers) {
            out += header.first + ": " + header.second + "\r\n";
        }
        for (const std::string& cookie : response.setCookies) {
            out += "Set-Cookie: " + cookie + "\r\n";
        }
        out += "\r\n";
        out += response.body;
        SocketSendAll(static_cast<intptr_t>(client), out);
    }

    void CloseWith(SOCKET client, int status) {
        HttpResponse r = HttpResponse::Text("error", status);
        SendResponse(client, r);
        closesocket(client);
    }

    void Response404(SOCKET client) {
        HttpResponse r = HttpResponse::Json(
            "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"接口不存在\"}}", 404);
        SendResponse(client, r);
        closesocket(client);
    }

    void AcceptLoop() {
        while (running.load()) {
            sockaddr_in addr{};
            int addrLen = sizeof(addr);
            SOCKET client = accept(listenSocket, reinterpret_cast<sockaddr*>(&addr), &addrLen);
            if (client == INVALID_SOCKET) {
                if (!running.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            char ip[64] = {0};
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            DWORD timeout = 15000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            std::string remoteIp(ip);
            std::thread([this, client, remoteIp]() {
                HandleConnection(client, remoteIp);
            }).detach();
        }
    }
};

HttpServer::HttpServer() : impl_(new Impl()) {}

HttpServer::~HttpServer() {
    Stop();
    delete impl_;
}

void HttpServer::Route(const std::string& method, const std::string& pattern, HttpHandler handler) {
    Impl::Route route;
    route.method = method;
    route.segments = SplitPath(pattern);
    route.handler = std::move(handler);
    impl_->routes.push_back(std::move(route));
}

void HttpServer::RouteStream(const std::string& method, const std::string& pattern, HttpStreamHandler handler) {
    Impl::StreamRoute route;
    route.method = method;
    route.segments = SplitPath(pattern);
    route.handler = std::move(handler);
    impl_->streamRoutes.push_back(std::move(route));
}

void HttpServer::SetFallback(HttpHandler handler) {
    impl_->fallback = std::move(handler);
}

bool HttpServer::Start(const std::string& bindAddress, uint16_t port, std::string& error) {
    if (impl_->running.load()) {
        error = "服务器已经在运行";
        return false;
    }
    if (!impl_->wsaStarted) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            error = "WSAStartup 失败";
            return false;
        }
        impl_->wsaStarted = true;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        error = "创建 socket 失败";
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bindAddress.empty() || bindAddress == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, bindAddress.c_str(), &addr.sin_addr) != 1) {
            error = "无效的绑定地址：" + bindAddress;
            closesocket(s);
            return false;
        }
    }
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = "端口绑定失败（可能被占用）：" + std::to_string(port);
        closesocket(s);
        return false;
    }
    if (listen(s, SOMAXCONN) != 0) {
        error = "listen 失败";
        closesocket(s);
        return false;
    }
    impl_->listenSocket = s;
    impl_->running.store(true);
    impl_->acceptThread = std::thread([this]() { impl_->AcceptLoop(); });
    return true;
}

void HttpServer::Stop() {
    if (!impl_->running.exchange(false)) {
        return;
    }
    if (impl_->listenSocket != INVALID_SOCKET) {
        closesocket(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
    }
    if (impl_->acceptThread.joinable()) {
        impl_->acceptThread.join();
    }
}

bool HttpServer::IsRunning() const {
    return impl_->running.load();
}

} // namespace lf