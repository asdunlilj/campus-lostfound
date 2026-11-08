param(
    [string]$BaseUrl = 'http://127.0.0.1:8080'
)
$ErrorActionPreference = 'Stop'
$base = $BaseUrl.TrimEnd('/')
$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
$loginBody = '{"studentNo":"admin","password":"admin123"}'
$login = Invoke-RestMethod -Method Post -Uri "$base/api/auth/login" -WebSession $session -Headers @{ 'X-Requested-With' = 'CampusLF' } -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($loginBody))
if (-not $login.ok) { throw '管理员登录失败（服务是否已启动？端口是否正确？）' }
Write-Host '管理员登录成功，开始创建演示数据...'

$items = @(
    @{ type = 1; title = '捡到一张校园卡'; description = '图书馆二楼自习区捡到，卡面姓王，请失主描述卡号后四位认领。'; categoryId = 1; locationId = 2; eventTime = '2026-11-08 14:30'; verifyQuestion = '卡号后四位是多少？'; expiresInDays = 30 },
    @{ type = 0; title = '丢失一副黑色耳机'; description = '昨天下午在体育馆打完球后不见了，充电盒上有贴纸，找到请联系我，谢谢！'; categoryId = 2; locationId = 5; eventTime = '2026-11-07 17:00'; verifyQuestion = ''; expiresInDays = 30 },
    @{ type = 1; title = '三食堂捡到一串钥匙'; description = '三食堂二楼靠窗座位捡到，有门禁卡和两把钥匙，已交到食堂服务台。'; categoryId = 3; locationId = 3; eventTime = '2026-11-08 12:10'; verifyQuestion = '钥匙串上有几张门禁卡？'; expiresInDays = 30 },
    @{ type = 0; title = '寻找蓝色保温杯'; description = '教学楼 A301 上完高数课后忘拿了，杯身贴着动漫贴纸。'; categoryId = 7; locationId = 1; eventTime = '2026-11-08 10:00'; verifyQuestion = ''; expiresInDays = 14 }
)

foreach ($it in $items) {
    $json = $it | ConvertTo-Json -Compress -Depth 5
    try {
        $resp = Invoke-RestMethod -Method Post -Uri "$base/api/items" -WebSession $session -Headers @{ 'X-Requested-With' = 'CampusLF' } -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($json))
        Write-Host ("  已创建: {0}  {1}" -f $resp.data.item.publicId, $resp.data.item.title)
    } catch {
        Write-Host ("  创建失败: {0}" -f $_.Exception.Message)
    }
}
Write-Host '演示数据创建完成。'
