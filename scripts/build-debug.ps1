#Requires -Version 5.1
<#
.SYNOPSIS
    Сборка FastBinary в Debug-конфигурации.

.DESCRIPTION
    Полный цикл: настройка (cmake --preset x64-debug), сборка в параллель и
    smoke-тест. Результат: out\build\x64-debug\FastBinary.exe.

    Окружение Visual Studio (cl.exe, INCLUDE/LIB, VSINSTALLDIR) подхватывается
    автоматически через vswhere + vcvarsall, поэтому окно
    "x64 Native Tools Command Prompt" не требуется — скрипт можно запускать
    из обычного PowerShell.

    Коды выхода: 0 — успех, 1 — ошибка (удобно для CI).

.PARAMETER SkipTests
    Не запускать smoke-тест после сборки.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build-debug.ps1
#>
[CmdletBinding()]
param(
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'out\build\x64-debug'

function Fail([string]$Message) {
    Write-Host "ОШИБКА: $Message" -ForegroundColor Red
    exit 1
}

function Import-VisualStudioEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        Fail 'vswhere.exe не найден — Visual Studio 2022+ не установлена?'
    }
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) {
        Fail 'Visual Studio с компонентом C++ (Microsoft.VisualStudio.Component.VC.Tools.x86.x64) не найдена.'
    }
    $vcvarsall = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvarsall)) {
        Fail "Не найден $vcvarsall"
    }

    # Переносим переменные окружения из vcvarsall в текущую сессию PowerShell.
    $envLines = cmd /d /c "call ""$vcvarsall"" x64 >nul 2>&1 && set"
    foreach ($line in $envLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
        }
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        Fail 'cl.exe недоступен после vcvarsall — проверьте установку MSVC.'
    }
    if (-not $env:VSINSTALLDIR) {
        # Пресетам CMake нужен VSINSTALLDIR (SegmentHeap.cmake).
        [Environment]::SetEnvironmentVariable('VSINSTALLDIR', "$vsRoot\", 'Process')
    }
    Write-Host "Visual Studio: $vsRoot" -ForegroundColor DarkGray
}

Import-VisualStudioEnvironment

Push-Location $repoRoot
try {
    Write-Host '==> Конфигурация CMake (пресет x64-debug)' -ForegroundColor Cyan
    & cmake --preset x64-debug
    if ($LASTEXITCODE -ne 0) {
        Fail "cmake --preset завершился с кодом $LASTEXITCODE"
    }

    Write-Host '==> Сборка Debug (out\build\x64-debug)' -ForegroundColor Cyan
    & cmake --build $buildDir --parallel
    if ($LASTEXITCODE -ne 0) {
        Fail "Сборка завершилась с кодом $LASTEXITCODE"
    }

    if (-not $SkipTests) {
        Write-Host '==> Smoke-тест (ctest -C Debug)' -ForegroundColor Cyan
        Push-Location $buildDir
        try {
            & ctest -C Debug --output-on-failure
            if ($LASTEXITCODE -ne 0) {
                Fail "Smoke-тест провалился (код $LASTEXITCODE)"
            }
        }
        finally {
            Pop-Location
        }
    }
}
finally {
    Pop-Location
}

Write-Host "OK: $buildDir\FastBinary.exe" -ForegroundColor Green
exit 0
