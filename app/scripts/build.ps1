param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sln = Join-Path $root 'CampusLostFound.sln'
if (-not (Test-Path $sln)) { throw "找不到解决方案: $sln" }

$pf86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
$candidates = @()
if ($pf86) {
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) { $candidates += (Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe') }
    }
    $candidates += (Join-Path $pf86 'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe')
    $candidates += (Join-Path $pf86 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe')
}
$candidates += 'D:\VS2022IDE\MSBuild\Current\Bin\MSBuild.exe'

$msbuild = $null
foreach ($c in $candidates) {
    if ($c -and (Test-Path $c)) { $msbuild = $c; break }
}
if (-not $msbuild) { throw '找不到 MSBuild.exe，请安装 Visual Studio 2022（C++ 桌面开发工作负载）' }

Write-Host "MSBuild: $msbuild"
& $msbuild $sln /p:Configuration=$Configuration /p:Platform=$Platform /m /v:m /nologo
if ($LASTEXITCODE -ne 0) { throw "编译失败，退出码 $LASTEXITCODE" }
Write-Host ""
Write-Host "编译成功: $root\$Platform\$Configuration\CampusLostFound.exe"
