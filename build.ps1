<#
    MzDebirth - Created by Simon Moscrop
    Free software under the GNU AGPL v3; see LICENSE.

    Build the rack, after holding the sequencer to its checks.

        .\build.ps1             the sequencer check, then the standalone app
        .\build.ps1 -NoApp      the check only
        .\build.ps1 -Release    and the files a release attaches, into dist\: the installer,
                                and a zip of the app with the Mz950's disks

    The engines are compiled from their own repositories, beside this one - ..\Mz101,
    ..\Mz303, ..\Mz808, ..\Mz909, ..\Mz950 and ..\MzPCF - and JUCE from ..\JUCE.

    cl.exe is never on the PATH - vswhere says where Visual Studio is, and cmake comes from
    the PATH or from inside Visual Studio.
#>
param(
    [switch]$NoApp,
    [switch]$Release
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "No Visual Studio found. Install VS Community with 'Desktop development with C++'."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                     -property installationPath
if (-not $vsPath) {
    Write-Error "Visual Studio is installed but the C++ tools are not. Add 'Desktop development with C++'."
}

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $bundled) { $cmake = $bundled }
}
if (-not $cmake) { Write-Error "No cmake on the PATH and none inside Visual Studio." }

$buildDir = Join-Path $root "build"

if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    Write-Host "configuring $buildDir (first time)"
    & $cmake -S $root -B $buildDir
    if ($LASTEXITCODE -ne 0) { Write-Error "cmake configure failed" }
}

# ---------------------------------------------------------------- the sequencer check

Write-Host "building the sequencer check"
& $cmake --build $buildDir --config Release --target SeqCheck -- /m /v:minimal 2>&1 |
    Where-Object { $_ -match "error|warning C" }
if ($LASTEXITCODE -ne 0) { Write-Error "the sequencer check did not build" }

& (Join-Path $buildDir "Release\SeqCheck.exe")
if ($LASTEXITCODE -ne 0) { Write-Error "the sequencer failed its checks - not building an app around it" }

if ($NoApp) { exit 0 }

# ---------------------------------------------------------------- the app

Write-Host ""
Write-Host "building the standalone"

# JUCE writes the Windows version resource once and never again, so when CMakeLists.txt has
# changed since, the old one is thrown away here.
$cmakeListsPath = Join-Path $root "CMakeLists.txt"
$rc = Join-Path $buildDir "MzDebirth_artefacts\JuceLibraryCode\MzDebirth_resources.rc"
if ((Test-Path $rc) -and (Get-Item $rc).LastWriteTime -lt (Get-Item $cmakeListsPath).LastWriteTime) {
    Remove-Item $rc
}

& $cmake --build $buildDir --config Release --target MzDebirth_Standalone -- /m /v:minimal 2>&1 |
    Where-Object { $_ -match "error|warning C" }
if ($LASTEXITCODE -ne 0) { Write-Error "the app build failed" }

# Report what is on disk, and when it was written, rather than that the build returned zero.
$exe = Join-Path $buildDir "MzDebirth_artefacts\Release\Standalone\MzDebirth.exe"
if (-not (Test-Path $exe)) { Write-Error "expected $exe and it is not there" }

$newest = Get-ChildItem (Join-Path $root "Source") -Recurse -Include *.cpp, *.h |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1
$f = Get-Item $exe
$stale = $f.LastWriteTime -lt $newest.LastWriteTime
Write-Host ""
Write-Host ("ok  -  {0,-14} {1,7} KB   {2}{3}" -f $f.Name, [math]::Round($f.Length / 1KB), $f.LastWriteTime,
            $(if ($stale) { "   *** OLDER THAN $($newest.Name) ***" } else { "" })) `
           -ForegroundColor $(if ($stale) { "Red" } else { "Green" })
if ($stale) { Write-Error "the app is older than the sources it was built from" }

if (-not $Release) { exit 0 }

# ---------------------------------------------------------------- the installer

$cmakeLists = Get-Content $cmakeListsPath -Raw
if ($cmakeLists -notmatch "project\(MzDebirth VERSION ([0-9.]+)") { Write-Error "no version in CMakeLists.txt" }
$version = $Matches[1]

# The disks the installer ships come from wherever cmake found Mz950.
$cache = Get-Content (Join-Path $buildDir "CMakeCache.txt") | Where-Object { $_ -like "MZ950_PATH:PATH=*" } | Select-Object -First 1
if (-not $cache -or $cache -notmatch "^MZ950_PATH:PATH=(.*)$") { Write-Error "no MZ950_PATH in the cmake cache" }
$disks = Join-Path ($Matches[1] -replace "/", "\") "disks"
if (-not (Test-Path (Join-Path $disks "DRUMS.hfe"))) { Write-Error "no Mz950 disks at $disks" }

$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
            Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { Write-Error "No Inno Setup 6. Install it from https://jrsoftware.org/isdl.php" }

Write-Host ""
Write-Host "building the installer for $version"

& $iscc /Q "/DAppVersion=$version" "/DDisks=$disks" (Join-Path $root "installer\MzDebirth.iss")
if ($LASTEXITCODE -ne 0) { Write-Error "the installer build failed" }

# ---------------------------------------------------------------- the zip, for running as it is

Write-Host "building the zip"

$dist = Join-Path $root "dist"
$stage = Join-Path $buildDir "zip"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage "disks") | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $disks "*.hfe") (Join-Path $stage "disks")
Copy-Item (Join-Path $root "LICENSE") $stage
$readme = (Get-Content (Join-Path $root "installer\README.txt") -Raw).Replace("@VERSION@", $version)
Set-Content (Join-Path $stage "README.txt") $readme -NoNewline

$zip = Join-Path $dist "MzDebirth-$version-win64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

# Report what is on disk, as for the build.
Write-Host ""
foreach ($name in "MzDebirth-$version-win64-Setup.exe", "MzDebirth-$version-win64.zip") {
    $f = Get-Item (Join-Path $dist $name)
    Write-Host ("ok  -  {0,-34} {1,5:N1} MB   {2}" -f $f.Name, ($f.Length / 1MB), $f.LastWriteTime) -ForegroundColor Green
}

exit 0
