# VizCommand ビルドスクリプト (PowerShell)
$ErrorActionPreference = "Stop"

$MSVC_VER  = "14.43.34808"
$VS_ROOT   = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$MSVC_BIN  = "$VS_ROOT\VC\Tools\MSVC\$MSVC_VER\bin\Hostx64\x64"
$MSVC_INC  = "$VS_ROOT\VC\Tools\MSVC\$MSVC_VER\include"
$MSVC_LIB  = "$VS_ROOT\VC\Tools\MSVC\$MSVC_VER\lib\x64"
$CMAKE_EXE = "$VS_ROOT\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$NINJA_EXE = "$VS_ROOT\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

# Windows SDK パスを検索
$sdkRoot    = "C:\Program Files (x86)\Windows Kits\10"
$sdkVersion = (Get-ChildItem "$sdkRoot\Include" | Sort-Object Name | Select-Object -Last 1).Name
Write-Host "Windows SDK: $sdkVersion"

$WIN_INC_UM    = "$sdkRoot\Include\$sdkVersion\um"
$WIN_INC_UCRT  = "$sdkRoot\Include\$sdkVersion\ucrt"
$WIN_INC_SHARE = "$sdkRoot\Include\$sdkVersion\shared"
$WIN_LIB_UM    = "$sdkRoot\Lib\$sdkVersion\um\x64"
$WIN_LIB_UCRT  = "$sdkRoot\Lib\$sdkVersion\ucrt\x64"

# Windows SDK bin パス
$WIN_BIN = "$sdkRoot\bin\$sdkVersion\x64"

# PATH に追加 (rc.exe / mt.exe / link.exe 等)
$env:PATH = "$MSVC_BIN;$WIN_BIN;$env:PATH"

# コンパイラ用インクルード/ライブラリパス
$env:INCLUDE = "$MSVC_INC;$WIN_INC_UCRT;$WIN_INC_UM;$WIN_INC_SHARE"
$env:LIB     = "$MSVC_LIB;$WIN_LIB_UCRT;$WIN_LIB_UM"

# build ディレクトリ作成
$buildDir = "$PSScriptRoot\build"
if (Test-Path "$buildDir\CMakeCache.txt") {
    Remove-Item "$buildDir\CMakeCache.txt" -Force
}
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
Set-Location $buildDir

# CMake 構成
Write-Host "--- CMake configure ---"
& $CMAKE_EXE -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$NINJA_EXE" `
    "-DCMAKE_CXX_COMPILER=$MSVC_BIN\cl.exe" `
    "-DCMAKE_BUILD_TYPE=Release" `
    $PSScriptRoot

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# Ninja ビルド
Write-Host "--- Ninja build ---"
& $NINJA_EXE -v
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host ""
Write-Host "Build succeeded: $buildDir\VizCommand.exe"
