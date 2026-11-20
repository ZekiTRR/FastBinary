#Requires -Version 5.1
<#
.SYNOPSIS
    Сборка FastBinary в Release-конфигурации и (опционально) zip-пакет для GitHub Releases.

.DESCRIPTION
    Полный цикл: настройка (cmake --preset x64-release), сборка в параллель и
    smoke-тест. Результат: out\build\x64-release\FastBinary.exe.

    С ключом -Package дополнительно собирается переносной архив в out\package:
    FastBinary.exe, Qt-библиотеки (windeployqt), MSVC-рантайм, Packers\ и README —
    формат прежних релизов (FastBinary-vX.Y-windows-x64.zip).

    Окружение Visual Studio (cl.exe, INCLUDE/LIB, VSINSTALLDIR) подхватывается
    автоматически через vswhere + vcvarsall, поэтому окно
    "x64 Native Tools Command Prompt" не требуется.

    Коды выхода: 0 — успех, 1 — ошибка (удобно для CI).

.PARAMETER SkipTests
    Не запускать smoke-тест после сборки.

.PARAMETER Package
    Собрать переносной zip-пакет в out\package.

.PARAMETER Version
    Версия для имени пакета (по умолчанию dev), например v0.2.0.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1 -Package -Version v0.2.0
#>
[CmdletBinding()]
param(
    [switch]$SkipTests,
    [switch]$Package,
    [string]$Version = 'dev'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'out\build\x64-release'

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

function Get-QtRoot {
    # QT_ROOT задаётся в CMakeLists.txt и попадает в кэш — берём оттуда.
    $cachePath = Join-Path $buildDir 'CMakeCache.txt'
    if (-not (Test-Path $cachePath)) {
        Fail 'CMakeCache.txt не найден — сначала выполните конфигурацию скриптом без -SkipTests.'
    }
    foreach ($line in Get-Content $cachePath) {
        if ($line -match '^QT_ROOT:PATH=(.+)$') {
            $qtRoot = $Matches[1]
            if (Test-Path (Join-Path $qtRoot 'bin\windeployqt.exe')) {
                return $qtRoot
            }
            Fail "В QT_ROOT ($qtRoot) нет bin\windeployqt.exe."
        }
    }
    Fail 'QT_ROOT не найден в CMakeCache.txt — задайте путь к Qt в CMakeLists.txt и перенастройте проект.'
}

function Copy-MsvcRuntime([string]$Stage) {
    # App-local копия MSVC-рантайма, как в прошлых релизах: VC\Redist\MSVC\<новый>\x64\Microsoft.VC*.CRT
    $redistRoot = Join-Path $env:VSINSTALLDIR 'VC\Redist\MSVC'
    if (-not (Test-Path $redistRoot)) {
        Fail "Каталог MSVC-редистрибьютива не найден: $redistRoot"
    }
    $crtDir = Get-ChildItem $redistRoot -Directory |
        Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Get-ChildItem (Join-Path $_.FullName 'x64') -Directory -ErrorAction SilentlyContinue } |
        Where-Object { $_.Name -match '^Microsoft\.VC\d+.*\.CRT$' } |
        Select-Object -First 1
    if (-not $crtDir) {
        Fail "В $redistRoot не найден Microsoft.VC*.CRT (x64)."
    }
    Copy-Item (Join-Path $crtDir.FullName '*.dll') $Stage
    Write-Host "MSVC-рантайм: $($crtDir.FullName)" -ForegroundColor DarkGray
}

function Build-ReleasePackage {
    $packageDir = Join-Path $repoRoot 'out\package'
    $stageName = "FastBinary-$Version-windows-x64"
    $stage = Join-Path $packageDir $stageName

    Write-Host "==> Пакет $stageName" -ForegroundColor Cyan

    if (-not (Test-Path (Join-Path $repoRoot 'Packers\xenolith.exe'))) {
        Fail 'Packers\xenolith.exe не найден — пакет без упаковщиков не имеет смысла.'
    }

    # ВАЖНО: не использовать Copy-Item без -Recurse для каталогов — в Windows
    # PowerShell 5.1 он создаёт пустую папку без содержимого. Для Packers — robocopy.
    Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $stage | Out-Null

    Copy-Item (Join-Path $buildDir 'FastBinary.exe') $stage
    Copy-Item (Join-Path $repoRoot 'README.md') $stage
    Copy-Item (Join-Path $repoRoot 'README.en.md') $stage

    robocopy (Join-Path $repoRoot 'Packers') (Join-Path $stage 'Packers') /E /NFL /NDL /NJH /NJS | Out-Null
    if ($LASTEXITCODE -ge 8) {
        Fail "robocopy Packers завершился с кодом $LASTEXITCODE"
    }
    $global:LASTEXITCODE = 0

    $qtRoot = Get-QtRoot
    & (Join-Path $qtRoot 'bin\windeployqt.exe') --release (Join-Path $stage 'FastBinary.exe')
    if ($LASTEXITCODE -ne 0) {
        Fail "windeployqt завершился с кодом $LASTEXITCODE"
    }

    # Приводим состав к формату прежних релизов: без переводов и программных GL.
    Remove-Item -Recurse -Force (Join-Path $stage 'translations') -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $stage 'D3Dcompiler_47.dll'), (Join-Path $stage 'opengl32sw.dll') -Force -ErrorAction SilentlyContinue

    Copy-MsvcRuntime $stage

    $zipPath = Join-Path $packageDir "$stageName.zip"
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    & tar -a -c -f $zipPath -C $packageDir $stageName
    if ($LASTEXITCODE -ne 0) {
        Fail "tar завершился с кодом $LASTEXITCODE"
    }

    Write-Host "Пакет собран: $zipPath" -ForegroundColor Green
    Get-FileHash $zipPath -Algorithm SHA256 | Format-List
}

Import-VisualStudioEnvironment

Push-Location $repoRoot
try {
    Write-Host '==> Конфигурация CMake (пресет x64-release)' -ForegroundColor Cyan
    & cmake --preset x64-release
    if ($LASTEXITCODE -ne 0) {
        Fail "cmake --preset завершился с кодом $LASTEXITCODE"
    }

    Write-Host '==> Сборка Release (out\build\x64-release)' -ForegroundColor Cyan
    & cmake --build $buildDir --parallel
    if ($LASTEXITCODE -ne 0) {
        Fail "Сборка завершилась с кодом $LASTEXITCODE"
    }

    if (-not $SkipTests) {
        Write-Host '==> Smoke-тест (ctest -C Release)' -ForegroundColor Cyan
        Push-Location $buildDir
        try {
            & ctest -C Release --output-on-failure
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

if ($Package) {
    Push-Location $repoRoot
    try {
        Build-ReleasePackage
    }
    finally {
        Pop-Location
    }
}
exit 0
