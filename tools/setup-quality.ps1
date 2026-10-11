$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
python -m pip install clang-format==19.1.7 clang-tidy==22.1.0 cmakelang==0.6.13 zizmor==1.30.1
if ($LASTEXITCODE -ne 0) { throw 'Analysis tool installation failed' }
$toolRoot = Join-Path $repoRoot 'build/tools/quality'
New-Item -ItemType Directory -Force $toolRoot | Out-Null
$actionlint = Join-Path $toolRoot 'actionlint.zip'
Invoke-WebRequest 'https://github.com/rhysd/actionlint/releases/download/v1.7.12/actionlint_1.7.12_windows_amd64.zip' -OutFile $actionlint
if ((Get-FileHash $actionlint -Algorithm SHA256).Hash -ne '6e7241b51e6817ea6a047693d8e6fed13b31819c9a0dd6c5a726e1592d22f6e9') { throw 'actionlint hash mismatch' }
Expand-Archive -LiteralPath $actionlint -DestinationPath $toolRoot -Force
$msi = Join-Path $toolRoot 'cppcheck.msi'
Invoke-WebRequest 'https://github.com/cppcheck-opensource/cppcheck/releases/download/2.21.0/cppcheck-2.21.0-x64-Setup.msi' -OutFile $msi
if ((Get-FileHash $msi -Algorithm SHA256).Hash -ne 'a86eef1180dc18fde46b08f4e4b12f2a1ead8cfc0dd6c294aac8757748cbbb24') { throw 'cppcheck hash mismatch' }
$destination = Join-Path $toolRoot 'cppcheck'
$process = Start-Process msiexec.exe -ArgumentList @('/a', "`"$msi`"", '/qn', "TARGETDIR=`"$destination`"") -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "cppcheck extraction failed: $($process.ExitCode)" }
$cppcheck = Get-ChildItem $destination -Filter cppcheck.exe -Recurse | Select-Object -First 1
if (-not $cppcheck) { throw 'cppcheck executable missing after extraction' }
if ($env:GITHUB_PATH) {
    # Only fixed repository-generated paths are exported, never workflow inputs.
    $toolRoot >> $env:GITHUB_PATH
    $cppcheck.DirectoryName >> $env:GITHUB_PATH
}
Write-Host "Quality tools provisioned in $toolRoot"
