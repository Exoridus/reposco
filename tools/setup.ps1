$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Set-Location $repoRoot
git submodule update --init third_party/openssl third_party/sqlcipher
if ($LASTEXITCODE -ne 0) { throw 'Dependency checkout failed' }
git submodule update --init --recursive third_party/qwindowkit
if ($LASTEXITCODE -ne 0) { throw 'Dependency checkout failed' }
$perlRoot = Join-Path $repoRoot 'build/tools/perl'
if (-not (Test-Path "$perlRoot/perl/bin/perl.exe")) {
    New-Item -ItemType Directory -Force build/tools | Out-Null
    $archive = Join-Path $repoRoot 'build/tools/perl.zip'
    Invoke-WebRequest 'https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54051_64bit/strawberry-perl-5.40.5.1-64bit-portable.zip' -OutFile $archive
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '6619fe7eeef921ccddb4aac3972fb602a0c690a3074205b863ade998d7bc79a6') {
        throw 'Strawberry Perl archive hash mismatch'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $perlRoot -Force
}
$oldLocale = $env:LC_ALL
try {
    $env:LC_ALL = 'C'
    & "$perlRoot/perl/bin/perl.exe" -v
    if ($LASTEXITCODE -ne 0) { throw 'Perl validation failed' }
} finally { $env:LC_ALL = $oldLocale }
