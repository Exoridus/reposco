param(
    [ValidateSet('Format', 'FormatCheck', 'Fast', 'Analysis')][string]$Mode = 'Fast',
    [string]$QtRoot = $env:REPOSCO_QT_ROOT
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
$env:PATH = "$repoRoot/build/tools/quality;$repoRoot/build/tools/quality/venv/Scripts;$env:PATH"
function Invoke-Tool([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}
$cppFiles = @(Get-ChildItem src, tests -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h' } | ForEach-Object FullName)
$qmlFiles = @(Get-ChildItem qml, tests/qml -Recurse -Filter *.qml | ForEach-Object FullName)
$cmakeFiles = @('CMakeLists.txt') + @(Get-ChildItem cmake, src, tests -Recurse -File | Where-Object { $_.Name -eq 'CMakeLists.txt' -or $_.Extension -eq '.cmake' } | ForEach-Object FullName)
if ($Mode -in 'Format', 'FormatCheck', 'Fast') {
    [string[]]$formatArgs = if ($Mode -eq 'Format') { @('-i') } else { @('--dry-run', '--Werror') }
    Invoke-Tool clang-format ($formatArgs + $cppFiles)
    $cmakeFormat = (Get-Command cmake-format -ErrorAction SilentlyContinue).Source
    if (-not $cmakeFormat) { $cmakeFormat = "$env:APPDATA/Python/Python312/Scripts/cmake-format.exe" }
    [string[]]$cmakeArgs = if ($Mode -eq 'Format') { @('-i') } else { @('--check') }
    Invoke-Tool $cmakeFormat ($cmakeArgs + $cmakeFiles)
    if ($QtRoot) {
        foreach ($file in $qmlFiles) {
            if ($Mode -eq 'Format') {
                Invoke-Tool "$QtRoot/bin/qmlformat.exe" @('-i', $file)
            } else {
                $formatted = & "$QtRoot/bin/qmlformat.exe" $file
                if ($LASTEXITCODE -ne 0) { throw "qmlformat failed for $file" }
                $expected = [IO.File]::ReadAllText($file).Replace("`r`n", "`n").TrimEnd()
                if (($formatted -join "`n").TrimEnd() -cne $expected) { throw "QML formatting differs: $file" }
            }
        }
    } elseif ($Mode -ne 'Fast') {
        throw 'QtRoot is required for QML formatting'
    } else {
        Write-Host 'QML formatting belongs to the Windows Qt lane; not run in the SDK-free lane.'
    }
}
if ($Mode -eq 'Fast') {
    & "$PSScriptRoot/check-repository.ps1"
    $cmakeLint = (Get-Command cmake-lint -ErrorAction SilentlyContinue).Source
    if (-not $cmakeLint) { $cmakeLint = "$env:APPDATA/Python/Python312/Scripts/cmake-lint.exe" }
    Invoke-Tool $cmakeLint $cmakeFiles
    Invoke-Tool actionlint @('-color')
    Invoke-Tool zizmor @('--offline', '--min-severity', 'low', '.github')
}
if ($Mode -eq 'Analysis') {
    $database = Join-Path $repoRoot 'build/windows-debug'
    $commands = Get-Content "$database/compile_commands.json" -Raw | ConvertFrom-Json
    $sourcePrefix = "$repoRoot/src/".Replace('\', '/')
    $testPrefix = "$repoRoot/tests/".Replace('\', '/')
    $ownedCommands = @($commands | Where-Object {
        $file = $_.file.Replace('\', '/')
        $file.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase) -or
            $file.StartsWith($testPrefix, [StringComparison]::OrdinalIgnoreCase)
    })
    $ownedSources = @($ownedCommands | ForEach-Object file | Sort-Object -Unique)
    if ($ownedSources.Count -eq 0) { throw 'No owned compile commands found' }
    foreach ($source in $ownedSources) { Invoke-Tool clang-tidy @('-p', $database, $source, '--extra-arg=/EHsc') }
    $cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
    $localCppcheck = Get-ChildItem "$repoRoot/build/tools/quality/cppcheck" -Recurse -Filter cppcheck.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    $cppcheckPath = if ($localCppcheck) { $localCppcheck.FullName } elseif ($cppcheck) { $cppcheck.Source } else { 'C:/Program Files/Cppcheck/cppcheck.exe' }
    $ownedDatabase = Join-Path $database 'owned_compile_commands.json'
    [IO.File]::WriteAllText($ownedDatabase, (ConvertTo-Json -InputObject $ownedCommands -Depth 8))
    Invoke-Tool $cppcheckPath @('--enable=warning,performance,portability', '--std=c++20', '--error-exitcode=1', '--library=qt', '--library=windows', '--inline-suppr', '--suppress=missingIncludeSystem', "--project=$ownedDatabase")
}
