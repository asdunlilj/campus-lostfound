#pragma once

namespace lf {

class HttpServer;

class Api {
public:
    // 注册全部 REST / SSE / 静态页面路由（必须在 HttpServer::Start 之前调用）
    static void RegisterRoutes(HttpServer& server);
    // 启动后台任务：过期扫描、会话清理
    static void StartBackgroundTasks();
};

} // namespace lf