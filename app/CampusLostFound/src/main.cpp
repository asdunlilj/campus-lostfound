#include "Api.h"
#include "Auth.h"
#include "Http.h"
#include "SelfTest.h"
#include "Store.h"
#include "Util.h"

using namespace lf;

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> gStopRequested{false};

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        gStopRequested.store(true);
        return TRUE;
    }
    return FALSE;
}

std::string ExeDir() {
    char buffer[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0) return ".";
    std::filesystem::path path(std::string(buffer, length));
    return path.parent_path().string();
}

std::vector<std::string> LocalIPv4Addresses() {
    std::vector<std::string> result;
    ULONG bufferSize = 16384;
    std::vector<unsigned char> buffer(bufferSize);
    auto* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG status = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &bufferSize);
    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        status = GetAdaptersAddresses(AF_INET, flags, nullptr, addresses, &bufferSize);
    }
    if (status != NO_ERROR) return result;
    for (auto* adapter = addresses; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr) continue;
            if (unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
            auto* sockaddr = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            char ip[64] = {0};
            if (inet_ntop(AF_INET, &sockaddr->sin_addr, ip, sizeof(ip)) == nullptr) continue;
            std::string text(ip);
            if (text == "127.0.0.1") continue;
            bool duplicate = false;
            for (const std::string& existing : result) {
                if (existing == text) duplicate = true;
            }
            if (!duplicate) result.push_back(text);
        }
    }
    return result;
}

void PrintBanner(int port, const std::string& dataDir, const std::string& initialAdminPassword) {
    std::printf("\n");
    std::printf("==============================================================\n");
    std::printf("  Campus Lost & Found  |  校园失物招领系统  v1.0\n");
    std::printf("==============================================================\n");
    std::printf("  本机访问:   http://127.0.0.1:%d\n", port);
    std::vector<std::string> ips = LocalIPv4Addresses();
    for (const std::string& ip : ips) {
        std::printf("  局域网访问: http://%s:%d   (手机需与本机同一网络)\n", ip.c_str(), port);
    }
    if (ips.empty()) {
        std::printf("  局域网访问: 未检测到 IPv4 地址，请检查网络连接\n");
    }
    std::printf("  数据目录:   %s\n", dataDir.c_str());
    if (!initialAdminPassword.empty()) {
        std::printf("  首次启动管理员: admin / %s\n", initialAdminPassword.c_str());
        std::printf("  ※ 该初始密码仅在本次启动显示，请立即登录并在“我的”中修改；\n");
        std::printf("    关闭窗口后无法找回（可删除 data 目录重新初始化，或联系管理员重置）。\n");
    } else {
        std::printf("  管理员账号: admin（密码为首次启动时设置/修改后的密码）\n");
    }
    std::printf("  邀请码:     登录管理员后台创建；完整邀请码仅在创建时显示一次\n");
    std::printf("  按 Ctrl+C 停止服务\n");
    std::printf("==============================================================\n\n");
    std::fflush(stdout);
}

int ParseArguments(int argc, char** argv, int& port, std::string& bindAddress, std::string& dataDir,
                   bool& selfTest) {
    port = 8080;
    bindAddress = "0.0.0.0";
    selfTest = false;
    dataDir = ExeDir() + "\\data";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest") {
            selfTest = true;
        } else if (arg == "--port" && i + 1 < argc) {
            int value = 0;
            if (!ParseInt(argv[++i], value) || value < 1 || value > 65535) {
                std::printf("无效的端口号\n");
                return 1;
            }
            port = value;
        } else if (arg == "--bind" && i + 1 < argc) {
            bindAddress = argv[++i];
        } else if (arg == "--data" && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::printf("用法: CampusLostFound.exe [--port 8080] [--bind 0.0.0.0] [--data <dir>] [--selftest]\n");
            return 0;
        } else {
            std::printf("未知参数: %s\n", arg.c_str());
            return 1;
        }
    }
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    int port = 8080;
    std::string bindAddress = "0.0.0.0";
    std::string dataDir;
    bool selfTest = false;
    int parseResult = ParseArguments(argc, argv, port, bindAddress, dataDir, selfTest);
    if (parseResult >= 0) return parseResult;

    if (selfTest) {
        return RunSelfTests(dataDir);
    }

    std::string error;
    if (!Store::Instance().Load(dataDir, error)) {
        std::printf("[错误] 数据加载失败: %s\n", error.c_str());
        return 1;
    }
    // 首次初始化时取出随机管理员密码（仅内存中保留一次，用于控制台提示）
    const std::string initialAdminPassword = Store::Instance().TakeInitialAdminPassword();

    HttpServer server;
    Api::RegisterRoutes(server);
    if (!server.Start(bindAddress, static_cast<uint16_t>(port), error)) {
        std::printf("[错误] 服务启动失败: %s\n", error.c_str());
        return 1;
    }
    Api::StartBackgroundTasks();
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    PrintBanner(port, Store::Instance().DataDir(), initialAdminPassword);

    while (!gStopRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::printf("\n正在停止服务...\n");
    server.Stop();
    std::printf("服务已停止。\n");
    return 0;
}