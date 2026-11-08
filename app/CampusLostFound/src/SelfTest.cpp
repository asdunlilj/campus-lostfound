#include "SelfTest.h"

#include "Auth.h"
#include "Json.h"
#include "Store.h"
#include "Util.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace lf {

namespace {

int gPassed = 0;
int gFailed = 0;

void Check(bool condition, const std::string& name, const std::string& detail = "") {
    if (condition) {
        ++gPassed;
        std::printf("  [PASS] %s\n", name.c_str());
    } else {
        ++gFailed;
        std::printf("  [FAIL] %s %s\n", name.c_str(), detail.c_str());
    }
}

void TestJson() {
    std::printf("\n[1] JSON 解析/序列化\n");
    JsonValue value;
    std::string error;
    bool parsed = JsonValue::Parse(
        "{\"title\":\"捡到校园卡\",\"ok\":true,\"n\":42,\"list\":[1,2,3],\"nested\":{\"a\":\"b\"}}", value, error);
    Check(parsed, "解析包含中文的 JSON", error);
    Check(value["title"].AsString() == "捡到校园卡", "读取中文字段");
    Check(value["ok"].AsBool(), "读取布尔字段");
    Check(value["n"].AsInt() == 42, "读取整数字段");
    Check(value["list"].Size() == 3, "读取数组长度");
    Check(value["nested"]["a"].AsString() == "b", "读取嵌套对象");
    JsonValue escaped;
    Check(JsonValue::Parse("{\"x\":\"a\\\"b\"}", escaped, error) && escaped["x"].AsString() == "a\"b",
          "转义字符串");
    std::string dumped = value.Dump();
    JsonValue round;
    Check(JsonValue::Parse(dumped, round, error), "序列化后重新解析");
    Check(round["title"].AsString() == "捡到校园卡", "往返中文一致");
    Check(round["list"].At(2).AsInt() == 3, "往返数组一致");
}

void TestPassword() {
    std::printf("\n[2] 密码哈希\n");
    std::string hash = Auth::Instance().HashPassword("test123456");
    Check(!hash.empty() && hash.find("pbkdf2-sha256$") == 0, "生成 PBKDF2 哈希");
    Check(Auth::Instance().VerifyPassword("test123456", hash), "正确密码校验通过");
    Check(!Auth::Instance().VerifyPassword("wrong-password", hash), "错误密码校验失败");
    std::string hash2 = Auth::Instance().HashPassword("test123456");
    Check(hash != hash2, "相同密码产生不同盐值");
}

void TestStoreFlow(const std::string& dataDir) {
    std::printf("\n[3] 数据层与业务闭环\n");
    Store& store = Store::Instance();
    std::string error;
    Check(store.Load(dataDir, error), "初始化数据目录", error);

    auto admin = store.FindUserByStudentNo("admin");
    Check(admin.has_value() && admin->role == "admin", "种子管理员存在");
    auto invite = store.FindInvite("CAMPUS-2026");
    Check(invite.has_value() && invite->maxUses > 0, "种子邀请码存在");

    std::string userError;
    int64_t user1 = store.CreateUser("20230001", "张三", "计算机学院",
                                     Auth::Instance().HashPassword("123456"), "student", userError);
    int64_t user2 = store.CreateUser("20230002", "李四", "外国语学院",
                                     Auth::Instance().HashPassword("123456"), "student", userError);
    Check(user1 > 0 && user2 > 0, "创建普通用户", userError);
    Check(!store.CreateUser("20230001", "重复用户", "计算机学院", "x", "student", userError),
          "重复学号被拒绝");

    auto user1Opt = store.FindUserById(user1);
    auto user2Opt = store.FindUserById(user2);
    Check(user1Opt.has_value() && user2Opt.has_value(), "按 ID 查找用户");

    JsonValue event = JsonValue::MakeObject();
    Item item;
    item.type = 1;
    item.userId = user1;
    item.title = "捡到一张校园卡";
    item.description = "图书馆二楼捡到，卡面有姓名。";
    item.categoryId = 1;
    item.locationId = 2;
    item.eventTime = "2026-11-08 14:30";
    item.verifyQuestion = "卡背面写了什么字？";
    item.expiresAt = NowEpoch() + 30 * 86400;
    std::string itemError;
    Check(store.CreateItem(item, itemError), "发布招领物品", itemError);
    Check(!item.publicId.empty(), "生成公开 ID");
    Check(store.FindItemByPublicId(item.publicId).has_value(), "按公开 ID 查询");

    Claim claim;
    claim.itemId = item.id;
    claim.claimantId = user2;
    claim.answer = "卡背面写了 2023";
    claim.message = "应该是我的";
    std::string claimError;
    Check(store.CreateClaim(claim, claimError), "提交认领申请", claimError);

    Claim claim2;
    claim2.itemId = item.id;
    claim2.claimantId = admin->id;
    claim2.answer = "乱答";
    std::string conflictError;
    Check(!store.CreateClaim(claim2, conflictError), "并发/重复认领被拒绝");

    Claim approved;
    std::string approveError;
    Check(store.ApproveClaim(claim.id, user1, approved, approveError), "发布者通过认领", approveError);
    Check(!approved.pickupCode.empty(), "生成 6 位取件码");
    auto itemAfterApprove = store.FindItemById(item.id);
    Check(itemAfterApprove.has_value() && itemAfterApprove->status == 2, "物品状态变为待交接");

    Claim wrongVerify;
    std::string verifyError;
    Check(!store.VerifyClaim(claim.id, user1, "000000", wrongVerify, verifyError), "错误取件码被拒绝");

    Claim completed;
    Check(store.VerifyClaim(claim.id, user1, approved.pickupCode, completed, verifyError),
          "正确取件码核销完成", verifyError);
    auto itemCompleted = store.FindItemById(item.id);
    Check(itemCompleted.has_value() && itemCompleted->status == 3, "物品完成");

    Item expired;
    expired.type = 0;
    expired.userId = user1;
    expired.title = "测试过期物品";
    expired.description = "用于验证过期扫描。";
    expired.categoryId = 7;
    expired.locationId = 7;
    expired.expiresAt = NowEpoch() - 10;
    std::string expiredError;
    Check(store.CreateItem(expired, expiredError), "创建已过期物品", expiredError);
    store.ExpireOverdue();
    auto expiredAfter = store.FindItemById(expired.id);
    Check(expiredAfter.has_value() && expiredAfter->status == 4, "过期扫描生效");

    int unreadBefore = store.UnreadCount(user2);
    store.AddNotification(user2, "test", "测试通知", "内容", "#/notifications");
    Check(store.UnreadCount(user2) == unreadBefore + 1, "新增通知后未读数增加");
    store.MarkNotificationsRead(user2, {}, true);
    Check(store.UnreadCount(user2) == 0, "全部标记已读");

    int64_t reportId = store.AddReport(user2, item.id, "测试举报");
    Check(reportId > 0, "提交举报");
    Check(!store.UpdateReportStatus(reportId, 1, admin->id, error) ? false : true, "处理举报");
    Check(store.GetStats().users >= 3, "统计接口返回用户数");

    std::string dump = store.DumpJson();
    Check(!dump.empty(), "导出 JSON 快照");
    std::string saveError;
    Check(store.Save(&saveError), "保存 db.json", saveError);
    Check(std::filesystem::exists(dataDir + "/db.json"), "db.json 文件存在");
}

} // namespace

int RunSelfTests(const std::string& requestedDir) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("==============================================\n");
    std::printf("  Campus Lost & Found 自测 (--selftest)\n");
    std::printf("==============================================\n");

    std::string dataDir = requestedDir;
    if (dataDir.empty()) {
        dataDir = (std::filesystem::current_path() / ("selftest_data_" + RandomHex(4))).string();
    }
    std::printf("数据目录: %s\n", dataDir.c_str());
    std::error_code ec;
    std::filesystem::create_directories(dataDir, ec);

    TestJson();
    TestPassword();
    TestStoreFlow(dataDir);

    std::filesystem::remove_all(dataDir, ec);

    std::printf("\n----------------------------------------------\n");
    std::printf("  通过: %d   失败: %d\n", gPassed, gFailed);
    std::printf("----------------------------------------------\n");
    return gFailed == 0 ? 0 : 1;
}

} // namespace lf