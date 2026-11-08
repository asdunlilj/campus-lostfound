#pragma once

#include <string>

namespace lf {

// 进程内自测（不启动网络），返回 0 = 全部通过。
// dataDir 为空时使用当前工作目录下的临时目录。
int RunSelfTests(const std::string& dataDir = "");

} // namespace lf