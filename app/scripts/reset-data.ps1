param(
    [string]$AppRoot = ''
)
$ErrorActionPreference = 'Stop'
if ($AppRoot -eq '') { $AppRoot = Split-Path -Parent $PSScriptRoot }
$targets = @(
    (Join-Path $AppRoot 'bin\Debug\data'),
    (Join-Path $AppRoot 'bin\Release\data'),
    (Join-Path $AppRoot 'x64\Debug\data'),
    (Join-Path $AppRoot 'x64\Release\data'),
    (Join-Path $AppRoot 'data')
)
$removed = 0
foreach ($t in $targets) {
    if (Test-Path $t) {
        Remove-Item $t -Recurse -Force
        Write-Host "已删除: $t"
        $removed++
    }
}
if ($removed -eq 0) { Write-Host '没有找到数据目录（可能还未运行过）' }
Write-Host '下次启动将重新初始化数据：控制台会打印一次新的随机管理员密码（admin），请立即保存并修改。'
