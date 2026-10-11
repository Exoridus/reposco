param(
    [Parameter(Position = 0)]
    [ValidateSet('setup', 'build', 'run', 'test', 'format', 'quick', 'full', 'clean')]
    [string]$Command = 'build',
    [string]$QtRoot = $env:REPOSCO_QT_ROOT,
    [string]$TestFilter = '.',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$UseSccache
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}

if ($Command -eq 'clean') {
    $buildRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build'))
    if ($buildRoot -ne "$repoRoot\build") { throw 'Unexpected clean path' }
    if (Test-Path $buildRoot) { Remove-Item -LiteralPath $buildRoot -Recurse -Force }
    return
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) { throw 'Install MSVC x64 tools or use a Developer PowerShell' }
    $environment = & cmd.exe /d /s /c "`"$vsRoot\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed' }
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
}
if (-not $QtRoot) {
    $version = (Get-Content .qt-version -Raw).Trim()
    $candidates = @($env:QT_ROOT_DIR, "C:/Qt/$version/msvc2022_64", "$env:USERPROFILE/Qt/$version/msvc2022_64")
    $QtRoot = $candidates | Where-Object { $_ -and (Test-Path "$_/bin/qmake.exe") } | Select-Object -First 1
}
if (-not $QtRoot -or -not (Test-Path "$QtRoot/bin/qmake.exe")) { throw 'Qt 6.12.0 missing. Pass -QtRoot <SDK path>' }
$QtRoot = (Resolve-Path $QtRoot).Path
$env:PATH = "$QtRoot/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$QtRoot/plugins"
$env:QML_IMPORT_PATH = "$QtRoot/qml"
$env:REPOSCO_QT_ROOT = $QtRoot

function Build-Preset([string]$Preset) {
    $configureArgs = @('--preset', $Preset, "-DCMAKE_PREFIX_PATH=$QtRoot")
    if ($UseSccache) { $configureArgs += '-DREPOSCO_USE_SCCACHE=ON' }
    Invoke-Checked cmake $configureArgs
    $clock = [Diagnostics.Stopwatch]::StartNew()
    Invoke-Checked cmake @('--build', '--preset', $Preset, '--parallel')
    Write-Host "$Preset build seconds: $($clock.Elapsed.TotalSeconds.ToString('F2'))"
    if ($UseSccache) { Invoke-Checked sccache @('--show-stats') }
}

switch ($Command) {
    'setup' { & "$PSScriptRoot/setup.ps1"; Invoke-Checked cmake @('--version'); Invoke-Checked cl @('/?') }
    'build' { Build-Preset "windows-$($Configuration.ToLowerInvariant())" }
    'run' { Build-Preset windows-debug; Invoke-Checked "$repoRoot/build/windows-debug/reposco.exe" @() }
    'test' { Build-Preset windows-debug; Invoke-Checked ctest @('--preset', 'windows-debug', '-R', $TestFilter) }
    'format' { & "$PSScriptRoot/quality.ps1" -Mode Format -QtRoot $QtRoot }
    'quick' {
        & "$PSScriptRoot/quality.ps1" -Mode FormatCheck -QtRoot $QtRoot
        Build-Preset windows-debug
        Invoke-Checked ctest @('--preset', 'windows-debug')
    }
    'full' {
        & "$PSScriptRoot/quality.ps1" -Mode Fast -QtRoot $QtRoot
        Build-Preset windows-debug
        Invoke-Checked cmake @('--build', '--preset', 'windows-debug', '--target', 'all_qmllint')
        Invoke-Checked ctest @('--preset', 'windows-debug')
        & "$PSScriptRoot/quality.ps1" -Mode Analysis -QtRoot $QtRoot
        Build-Preset windows-release
        & "$PSScriptRoot/package.ps1" -QtRoot $QtRoot
    }
}
