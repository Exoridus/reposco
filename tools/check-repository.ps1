$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
$files = @(Get-ChildItem src, tests, qml, cmake, tools -Recurse -File)
foreach ($file in $files) {
    $text = [IO.File]::ReadAllText($file.FullName)
    $relativePath = [IO.Path]::GetRelativePath($repoRoot, $file.FullName).Replace('\', '/')
    if ($text.Contains("`r")) { throw "Expected LF: $($file.FullName)" }
    if ($text -match '[\u2013\u2014\u2018\u2019\u201c\u201d\u2026]') { throw "Non-ASCII punctuation: $($file.FullName)" }
    if ($text -match '(?m)\bNOLINT\b(?![^\n]* -- .+)') { throw "Undocumented suppression: $($file.FullName)" }
    if ($relativePath.StartsWith('src/core/') -and $text -match '#\s*include\s*[<"](windows.h|sqlite3.h|QWK|QQuick|QWidget)') { throw 'Core boundary violation' }
    if ($file.Extension -eq '.h' -and $text -match 'using namespace') { throw "Header namespace pollution: $($file.Name)" }
}
$lock = Get-Content third_party/deps.lock.json -Raw | ConvertFrom-Json
foreach ($dependency in $lock.sources) {
    if ($dependency.revision -notmatch '^[0-9a-f]{40}$') { throw "Unpinned dependency $($dependency.name)" }
    $head = git -C $dependency.path rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head -ne $dependency.revision) { throw "Dependency revision mismatch: $($dependency.name)" }
    if (-not (Test-Path "$($dependency.path)/$($dependency.licenseFile)")) { throw "Missing license: $($dependency.name)" }
    $dirty = git -C $dependency.path status --porcelain --untracked-files=no
    if ($dirty) { throw "Modified dependency: $($dependency.name)" }
}
if ((Get-Content .qt-version -Raw).Trim() -ne $lock.qt.version) { throw 'Qt lock mismatch' }
if (-not (Test-Path LICENSE)) { throw 'Missing project license' }
git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Whitespace check failed' }
Write-Host "Repository hygiene and $($lock.sources.Count) dependency/license records passed."
