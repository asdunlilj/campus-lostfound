param(
    [string]$BaseUrl = 'http://127.0.0.1:8080'
)
$ErrorActionPreference = 'Stop'
$base = $BaseUrl.TrimEnd('/')
$script:passed = 0
$script:failed = 0
function Check([bool]$cond, [string]$name, [string]$detail = '') {
    if ($cond) { $script:passed++; Write-Host "  [PASS] $name" }
    else { $script:failed++; Write-Host "  [FAIL] $name  $detail" }
}
function Get-Json([string]$path, $session) {
    $argList = @{ Method = 'Get'; Uri = "$base$path" }
    if ($session) { $argList.WebSession = $session }
    return Invoke-RestMethod @argList
}
function Post-Json([string]$path, $body, $session) {
    $json = $body | ConvertTo-Json -Depth 8 -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $argList = @{ Method = 'Post'; Uri = "$base$path"; Headers = @{ 'X-Requested-With' = 'CampusLF' }; ContentType = 'application/json; charset=utf-8'; Body = $bytes }
    if ($session) { $argList.WebSession = $session }
    return Invoke-RestMethod @argList
}
function Post-ExpectFail([string]$path, $body, $session, [int]$expectStatus) {
    try {
        Post-Json $path $body $session | Out-Null
        return $false
    } catch {
        return [int]$_.Exception.Response.StatusCode -eq $expectStatus
    }
}

Write-Host ''
Write-Host '== 1. 健康检查与静态页面 =='
$health = Get-Json '/api/health' $null
Check ($health.ok -eq $true) '健康检查 /api/health'
$page = Invoke-WebRequest -Uri "$base/" -UseBasicParsing
Check ($page.StatusCode -eq 200 -and $page.Content.Length -gt 1000) "SPA 首页 ($($page.Content.Length) bytes)"
Check (($page.Headers['Content-Type'] -join ';') -like '*text/html*') '首页 Content-Type'
$meta = Get-Json '/api/meta' $null
Check ($meta.ok -and $meta.data.categories.Count -ge 7) '元数据分类'

Write-Host ''
Write-Host '== 2. 管理员登录 =='
$admin = New-Object Microsoft.PowerShell.Commands.WebRequestSession
$adminLogin = Post-Json '/api/auth/login' @{ studentNo = 'admin'; password = 'admin123' } $admin
Check ($adminLogin.ok -and $adminLogin.data.role -eq 'admin') '管理员登录'
$me = Get-Json '/api/auth/me' $admin
Check ($me.data.user.studentNo -eq 'admin') '管理员会话'

Write-Host ''
Write-Host '== 3. 发布物品 =='
$itemResp = Post-Json '/api/items' @{
    type = 1
    title = '捡到一张校园卡'
    description = '图书馆二楼捡到，卡面姓名王*，请失主联系。'
    categoryId = 1
    locationId = 2
    eventTime = '2026-11-08 14:30'
    verifyQuestion = '卡背面写了什么字？'
    expiresInDays = 30
} $admin
$item = $itemResp.data.item
Check ($itemResp.ok -and $item.publicId.Length -ge 8) "发布招领物品 ($($item.publicId))"
Check ($item.status -eq 0 -and $item.statusText -eq '进行中') '物品初始状态'
$list = Get-Json '/api/items?q=%E6%A0%A1%E5%9B%AD%E5%8D%A1' $null
Check ($list.data.total -ge 1) "关键词搜索命中 $($list.data.total) 条"

Write-Host ''
Write-Host '== 4. 学生注册与认领 =='
$stu = New-Object Microsoft.PowerShell.Commands.WebRequestSession
$reg = Post-Json '/api/auth/register' @{ inviteCode = 'CAMPUS-2026'; studentNo = '20230001'; name = '李四'; college = '外国语学院'; password = '123456' } $stu
Check ($reg.ok -and $reg.data.role -eq 'student') '邀请码注册学生'
$detail = Get-Json "/api/items/$($item.publicId)" $stu
Check ($detail.data.item.claimAllowed -eq $true) '学生可认领'
$claimResp = Post-Json "/api/items/$($item.publicId)/claims" @{ answer = '卡背面写了2023'; message = '应该是我的，谢谢！' } $stu
$claim = $claimResp.data.claim
Check ($claimResp.ok -and $claim.status -eq 0) "提交认领申请 #$($claim.id)"
$dup = Post-ExpectFail "/api/items/$($item.publicId)/claims" @{ answer = '再试一次'; message = '重复申请' } $stu 409
Check $dup '重复认领被拒绝(409)'

Write-Host ''
Write-Host '== 5. 审核 / 取件码 / 核销 =='
$claims = Get-Json "/api/items/$($item.publicId)/claims" $admin
Check ($claims.data.claims.Count -eq 1 -and $claims.data.claims[0].canReview -eq $true) '发布者查看可审核申请'
$approve = Post-Json "/api/claims/$($claim.id)/approve" @{} $admin
Check ($approve.ok -and $approve.data.pickupCode.Length -eq 6) "审核通过并生成取件码 $($approve.data.pickupCode)"
$wrong = Post-ExpectFail "/api/claims/$($claim.id)/verify" @{ code = '000000' } $admin 409
Check $wrong '错误取件码被拒绝(409)'
$verify = Post-Json "/api/claims/$($claim.id)/verify" @{ code = $approve.data.pickupCode } $admin
Check ($verify.ok -and $verify.data.claim.status -eq 5) '正确取件码核销完成'
$itemAfter = Get-Json "/api/items/$($item.publicId)" $admin
Check ($itemAfter.data.item.status -eq 3) '物品状态已完成'
$myClaims = Get-Json '/api/me/claims' $stu
Check ($myClaims.data.claims.Count -eq 1 -and $myClaims.data.claims[0].pickupCode -eq $approve.data.pickupCode) '学生可见取件码'

Write-Host ''
Write-Host '== 6. 通知 / 权限 / 管理 =='
$notif = Get-Json '/api/notifications' $stu
Check ($notif.data.unreadCount -ge 1) "学生未读通知 $($notif.data.unreadCount)"
$read = Post-Json '/api/notifications/read' @{ all = $true } $stu
Check ($read.data.unreadCount -eq 0) '全部标记已读'
$forbidden = $false
try { Get-Json "/api/items/$($item.publicId)/claims" $stu | Out-Null } catch { $forbidden = ([int]$_.Exception.Response.StatusCode -eq 403) }
Check $forbidden '学生查看他人认领申请被拒(403)'
$adminStu = $false
try { Get-Json '/api/admin/stats' $stu | Out-Null } catch { $adminStu = ([int]$_.Exception.Response.StatusCode -eq 403) }
Check $adminStu '学生访问管理接口被拒(403)'
$stats = Get-Json '/api/admin/stats' $admin
Check ($stats.data.stats.users -ge 2 -and $stats.data.stats.completedClaims -ge 1) "管理统计 users=$($stats.data.stats.users) completed=$($stats.data.stats.completedClaims)"
$invite = Post-Json '/api/admin/invites' @{ maxUses = 10; validDays = 7 } $admin
Check ($invite.ok -and $invite.data.invite.code.Length -ge 6) "创建邀请码 $($invite.data.invite.code)"
$report = Post-Json '/api/reports' @{ targetPublicId = $item.publicId; reason = '测试举报内容' } $stu
Check ($report.ok -and $report.data.reported -eq $true) '提交举报'
$reports = Get-Json '/api/admin/reports' $admin
Check ($reports.data.reports.Count -ge 1) '管理员查看举报'
$handle = Post-Json "/api/admin/reports/$($reports.data.reports[0].id)/handle" @{ action = 'resolve' } $admin
Check ($handle.ok -and $handle.data.report.status -eq 1) '处理举报'

Write-Host ''
Write-Host '== 7. SSE 实时流握手 =='
$cookie = $admin.Cookies.GetCookies($base)['lf_sid']
Check ($cookie -ne $null) '管理员会话 Cookie 存在'
if ($cookie -ne $null) {
    try {
        $uri = [Uri]$base
        $tcp = New-Object System.Net.Sockets.TcpClient($uri.Host, $uri.Port)
        $stream = $tcp.GetStream()
        $crlf = [string][char]13 + [string][char]10
        $hostHeader = 'Host: ' + $uri.Host + ':' + $uri.Port
        $request = 'GET /api/stream HTTP/1.1' + $crlf + $hostHeader + $crlf + 'Cookie: lf_sid=' + $cookie.Value + $crlf + 'Accept: text/event-stream' + $crlf + $crlf
        $bytes = [System.Text.Encoding]::ASCII.GetBytes($request)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.ReadTimeout = 3000
        $buffer = New-Object byte[] 4096
        $text = ''
        $deadline = (Get-Date).AddSeconds(3)
        while ((Get-Date) -lt $deadline) {
            try { $n = $stream.Read($buffer, 0, $buffer.Length) } catch { break }
            if ($n -le 0) { break }
            $text += [System.Text.Encoding]::ASCII.GetString($buffer, 0, $n)
            if ($text -like 'HTTP/1.1 200*') { break }
        }
        $stream.Close(); $tcp.Close()
        Check ($text -like '*text/event-stream*') 'SSE 返回 text/event-stream'
    } catch {
        Check $false 'SSE 握手' $_.Exception.Message
    }
}

Write-Host ''
Write-Host '== 8. 退出登录 =='
$logout = Post-Json '/api/auth/logout' @{} $stu
Check ($logout.ok) '注销'
$unauth = $false
try { Get-Json '/api/auth/me' $stu | Out-Null } catch { $unauth = ([int]$_.Exception.Response.StatusCode -eq 401) }
Check $unauth '注销后接口返回 401'

Write-Host ''
Write-Host '=========================================='
Write-Host "  通过: $script:passed   失败: $script:failed"
Write-Host '=========================================='
if ($script:failed -gt 0) { exit 1 } else { exit 0 }
