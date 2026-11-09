param(
    [string]$AppRoot = '',
    [string]$DataDir = ''
)
$ErrorActionPreference = 'Stop'
if ($AppRoot -eq '') { $AppRoot = Split-Path -Parent $PSScriptRoot }

$targets = @()
if ($DataDir -ne '') {
    $targets += $DataDir
} else {
    $targets += (Join-Path $AppRoot 'bin\Debug\data')
    $targets += (Join-Path $AppRoot 'bin\Release\data')
    $targets += (Join-Path $AppRoot 'x64\Debug\data')
    $targets += (Join-Path $AppRoot 'x64\Release\data')
    $targets += (Join-Path $AppRoot 'data')
}
$targets = $targets | Where-Object { Test-Path $_ } | Select-Object -Unique
if (-not $targets) { throw '没有找到数据目录，请先启动一次服务或通过 -DataDir 指定' }

$identity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
foreach ($dir in $targets) {
    Write-Host "收紧 ACL: $dir"
    icacls $dir /inheritance:r | Out-Null
    icacls $dir /grant:r "${identity}:(OI)(CI)F" "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" /T | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "  失败（退出码 $LASTEXITCODE），请以管理员身份运行" }
    else { Write-Host '  完成：仅当前用户 / SYSTEM / Administrators 可访问' }
}
Write-Host '注意：数据目录包含 db.json（密码哈希、邀请码哈希、学号姓名等个人信息）与上传图片，'
Write-Host '      请勿把 data 目录放入 Git、网盘或公开共享目录；备份时同样按敏感数据对待。'
