param(
    [int]$Port = 8080,
    [string]$DataDir = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = @(
    (Join-Path $root 'bin\Debug\CampusLostFound.exe'),
    (Join-Path $root 'bin\Release\CampusLostFound.exe'),
    (Join-Path $root 'x64\Debug\CampusLostFound.exe'),
    (Join-Path $root 'x64\Release\CampusLostFound.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw '找不到 CampusLostFound.exe，请先编译（scripts\build.ps1 或在 VS 中生成）' }

$args = @('--port', "$Port")
if ($DataDir -ne '') { $args += @('--data', $DataDir) }

Write-Host "启动: $exe $($args -join ' ')"
Write-Host "访问: http://127.0.0.1:$Port"
& $exe @args
