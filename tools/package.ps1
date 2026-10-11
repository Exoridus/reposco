param([string]$QtRoot = $env:REPOSCO_QT_ROOT, [switch]$Installer)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
$stage = Join-Path $repoRoot 'build/package-stage'
if (Test-Path $stage) {
    if ([IO.Path]::GetFullPath($stage) -ne "$repoRoot\build\package-stage") { throw 'Unexpected staging path' }
    Remove-Item -LiteralPath $stage -Recurse -Force
}
cmake --install build/windows-release --prefix $stage --component Runtime
if ($LASTEXITCODE -ne 0) { throw 'Deployment failed' }
if (-not (Test-Path "$stage/reposco.exe") -or -not (Test-Path "$stage/Qt6Core.dll")) { throw 'Incomplete deployment' }
foreach ($runtime in @('msvcp140.dll', 'vcruntime140.dll')) {
    if (-not (Test-Path "$stage/$runtime")) { throw "Missing application-local runtime: $runtime" }
}
if (-not $QtRoot) { $QtRoot = $env:QT_ROOT_DIR }
$qtLicenses = Join-Path $repoRoot 'packaging/licenses/Qt'
if (-not (Test-Path $qtLicenses)) { throw 'Qt SDK license texts missing' }
Copy-Item -LiteralPath $qtLicenses -Destination "$stage/licenses/Qt" -Recurse
foreach ($notice in @('third_party/openssl/NOTICE', 'third_party/qwindowkit/NOTICE')) {
    if (Test-Path $notice) { Copy-Item -LiteralPath $notice -Destination "$stage/licenses/$(Split-Path (Split-Path $notice -Parent) -Leaf)-NOTICE.txt" }
}
$oldPath = $env:PATH
$oldPlugins = $env:QT_PLUGIN_PATH
$oldImports = $env:QML_IMPORT_PATH
try {
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = "$stage/plugins"
    $env:QML_IMPORT_PATH = "$stage/qml"
    $process = Start-Process -FilePath "$stage/reposco.exe" -ArgumentList '--smoke' -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(15000)) { $process.Kill(); throw 'Packaged app timed out' }
    if ($process.ExitCode -ne 0) { throw "Packaged app failed: $($process.ExitCode)" }
} finally {
    $env:PATH = $oldPath
    $env:QT_PLUGIN_PATH = $oldPlugins
    $env:QML_IMPORT_PATH = $oldImports
}
$zip = Join-Path $repoRoot 'build/Reposco-0.1.0-dev-windows-x64.zip'
Compress-Archive -Path "$stage/*" -DestinationPath $zip -Force
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zip.sha256", "$hash  $([IO.Path]::GetFileName($zip))`n")
Write-Host "Package smoke passed. ZIP bytes: $((Get-Item $zip).Length); SHA256: $hash"
if ($Installer) {
    if (-not (Get-Command makensis.exe -ErrorAction SilentlyContinue)) { throw 'NSIS 3 required for -Installer' }
    & makensis.exe "/DSTAGE_ROOT=$stage" "/DOUTPUT=$repoRoot/build/Reposco-0.1.0-dev-setup.exe" packaging/reposco.nsi
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
}
