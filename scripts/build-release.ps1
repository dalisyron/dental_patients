# Build a release binary, deploy Qt DLLs, and produce the installer.
# Run after scripts\setup-windows.ps1.
#
# Output: installer\Output\DentalPatients-Setup-<version>.exe
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$buildDir = Join-Path $repoRoot 'build\release'
$qtRoot   = 'C:\Qt\6.10.3\msvc2022_64'

$toolPaths = @(
    (Join-Path $env:ProgramFiles 'CMake\bin'),
    (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'),
    (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6')
)
foreach ($p in $toolPaths) {
    if ((Test-Path $p) -and (($env:Path -split ';') -notcontains $p)) {
        $env:Path = "$p;$env:Path"
    }
}

if (-not (Test-Path $qtRoot)) {
    throw "Qt not found at $qtRoot. Run scripts\setup-windows.ps1 first."
}

# 1. Locate VS Build Tools developer environment.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere not found - VS Build Tools missing."
}
$vsRoot = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Workload.VCTools `
    -property installationPath
if (-not $vsRoot) { throw 'No VS install with C++ workload found.' }

# 2. Import vcvars64 into the current PowerShell session.
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }
$tmp = New-TemporaryFile
cmd /c "`"$vcvars`" >nul && set" | Out-File -FilePath $tmp -Encoding ascii
foreach ($line in Get-Content $tmp) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}
Remove-Item $tmp

# 3. Configure + build.
$env:Path = "$qtRoot\bin;$env:Path"
$env:CMAKE_PREFIX_PATH = $qtRoot

if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

& cmake -S $repoRoot -B $buildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$qtRoot"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& cmake --build $buildDir --target DentalPatients
if ($LASTEXITCODE -ne 0) { throw "build failed" }

# 4. Run windeployqt into build\release\dist.
& cmake --build $buildDir --target deploy
if ($LASTEXITCODE -ne 0) { throw "deploy failed" }

# 5. Build installer.
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
}
if (-not (Test-Path $iscc)) {
    $iscc = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'
}
if (-not (Test-Path $iscc)) { throw 'Inno Setup not found.' }

# Read version straight from CMakeLists.txt so the installer filename matches.
$cmake = Get-Content (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(DentalPatients[\s\S]*?VERSION\s+(\d+\.\d+\.\d+)') {
    throw 'could not parse version from CMakeLists.txt'
}
$version = $matches[1]

$distDir = Join-Path $buildDir 'dist'
& $iscc "/DAppVersion=$version" "/DSourceDir=$distDir" (Join-Path $repoRoot 'installer\DentalPatients.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }

$out = Join-Path $repoRoot "installer\Output\DentalPatients-Setup-$version.exe"
Write-Host ''
Write-Host "Installer ready: $out"
