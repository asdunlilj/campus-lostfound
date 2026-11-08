#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace lf {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string queryString;
    std::map<std::string, std::string> headers;  // key 全部小写
    std::string body;
    std::map<std::string, std::string> query;    // 已经 URL 解码
    std::map<std::string, std::string> pathParams; // 路由 {id} 参数
    std::string remoteIp;

    struct Upload {
        std::string fieldName;
        std::string fileName;
        std::string contentType;
        std::string data;
    };
    std::vector<Upload> uploads;

    std::string QueryValue(const std::string& key, const std::string& def = "") const;
    std::string HeaderValue(const std::string& key, const std::string& def = "") const;
    std::string CookieValue(const std::string& key) const;
    std::string PathValue(const std::string& key, const std::string& def = "") const;
    bool IsMultipart() const;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::string> setCookies;

    static HttpResponse Json(const std::string& body, int status = 200);
    static HttpResponse Text(const std::string& body, int status = 200);
    static HttpResponse Html(const std::string& body, int status = 200);
    static HttpResponse Binary(const std::string& body, const std::string& contentType);

    // maxAgeSeconds: 0 表示会话 Cookie；< 0 表示立即过期（清除）
    void AddCookie(const std::string& name, const std::string& value, int maxAgeSeconds,
                   bool httpOnly, bool secure);
    void ClearCookie(const std::string& name);
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;
// SSE 流式处理：handler 内部阻塞读写该 socket，返回时服务端关闭连接
using HttpStreamHandler = std::function<void(const HttpRequest&, intptr_t socketHandle)>;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // pattern 示例： /api/items/{id}  ，{id} 匹配单段路径
    void Route(const std::string& method, const std::string& pattern, HttpHandler handler);
    void RouteStream(const std::string& method, const std::string& pattern, HttpStreamHandler handler);
    // 未匹配时的兜底（例如 SPA 首页、404 JSON）
    void SetFallback(HttpHandler handler);

    bool Start(const std::string& bindAddress, uint16_t port, std::string& error);
    void Stop();
    bool IsRunning() const;

private:
    struct Impl;
    Impl* impl_;
};

// 供 SSE 使用：可靠地发送全部字节；失败返回 false
bool SocketSendAll(intptr_t socketHandle, const std::string& data);

} // namespace lf