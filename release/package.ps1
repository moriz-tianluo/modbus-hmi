#Requires -Version 5.1
<#
.SYNOPSIS
    构建 Release 版本，并把 exe、Qt 依赖与语言包打包为 zip。

.DESCRIPTION
    默认流程：配置（首次）/ 构建 Release -> 复制到 release/dist/<名称> ->
    windeployqt 收集 Qt 运行库 -> 复制语言包与说明 -> 压缩为 zip。
    所有产物均输出到脚本所在目录的 dist 子目录。

.PARAMETER Version
    版本号，用于生成目录名与压缩包名，如 1.0.0。

.PARAMETER BuildDir
    CMake 构建目录，默认为仓库下的 temp/build-verify。

.PARAMETER QtDir
    Qt 6 安装目录（包含 bin/windeployqt.exe）。

.PARAMETER SkipBuild
    跳过构建，直接对已有 Release 产物打包。

.PARAMETER SkipZip
    只生成打包目录，不压缩。

.EXAMPLE
    .\package.ps1 -Version 1.0.0

.EXAMPLE
    .\package.ps1 -Version 1.0.1 -SkipBuild
#>
[CmdletBinding()]
param(
    [string]$Version = "0.9.0",
    [string]$BuildDir = "",
    [string]$QtDir = "D:\QT6.8\QT6.8\6.8.3\msvc2022_64",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$SkipBuild,
    [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

# ---------- 路径解析 ----------
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "temp\build-verify"
}
$DistRoot = Join-Path $PSScriptRoot "dist"
$StageName = "modbus-hmi-v$Version-win64"
$StageDir = Join-Path $DistRoot $StageName
$ZipPath = Join-Path $DistRoot "$StageName.zip"
$ExeName = "modbus-hmi.exe"
$Windeployqt = Join-Path $QtDir "bin\windeployqt.exe"

function Assert-Environment {
    if (-not (Test-Path $Windeployqt)) {
        throw "未找到 windeployqt：$Windeployqt（请用 -QtDir 指定 Qt 安装目录）"
    }
}

function Invoke-ReleaseBuild {
    if ($SkipBuild) {
        Write-Host "[skip] 跳过构建"
        return
    }

    if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        Write-Host "[cmake] 配置构建目录：$BuildDir"
        & cmake -S $RepoRoot -B $BuildDir -G $Generator -A x64 "-DCMAKE_PREFIX_PATH=$QtDir"
        if ($LASTEXITCODE -ne 0) {
            throw "CMake 配置失败"
        }
    }

    Write-Host "[build] 编译 Release"
    & cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "Release 编译失败"
    }
}

function Invoke-Deploy {
    param([string]$ExePath)

    if (Test-Path $StageDir) {
        Remove-Item $StageDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
    Copy-Item $ExePath $StageDir -Force

    Write-Host "[deploy] windeployqt 收集 Qt 依赖"
    & $Windeployqt --release --compiler-runtime --dir $StageDir (Join-Path $StageDir $ExeName)
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt 执行失败"
    }

    Copy-Item (Join-Path $RepoRoot "language") $StageDir -Recurse -Force
    Copy-Item (Join-Path $RepoRoot "ReadMe.md") $StageDir -Force

    $versionFile = Join-Path $StageDir "VERSION.txt"
    $lines = @(
        "version=$Version",
        "built=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    )
    Set-Content -Path $versionFile -Value $lines -Encoding UTF8
}

function New-ZipPackage {
    if ($SkipZip) {
        Write-Host "[skip] 跳过压缩"
        return
    }
    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }

    Write-Host "[zip] 生成压缩包"
    Compress-Archive -Path $StageDir -DestinationPath $ZipPath -CompressionLevel Optimal
}

Assert-Environment
Invoke-ReleaseBuild

$exePath = Join-Path (Join-Path $BuildDir "Release") $ExeName
if (-not (Test-Path $exePath)) {
    throw "未找到 Release 可执行文件：$exePath"
}

Invoke-Deploy -ExePath $exePath
New-ZipPackage

Write-Host ""
Write-Host "打包完成"
Write-Host "  目录：$StageDir"
if (-not $SkipZip) {
    $zipInfo = Get-Item $ZipPath
    Write-Host ("  压缩包：{0}（{1:N2} MB）" -f $zipInfo.FullName, ($zipInfo.Length / 1MB))
}
