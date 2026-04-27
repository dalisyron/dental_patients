# Configure + build + run unit tests. Mirrors build-release.ps1's environment
# setup but builds in build\tests and uses ctest.
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$buildDir = Join-Path $repoRoot 'build\tests'
$qtRoot   = 'C:\Qt\6.10.3\msvc2022_64'

$toolPaths = @(
    (Join-Path $env:ProgramFiles 'CMake\bin'),
    (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'),
    (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe'),
    # vcvars64.bat invokes vswhere.exe by name, so the Installer dir must be on PATH.
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer')
)
foreach ($p in $toolPaths) {
    if ((Test-Path $p) -and (($env:Path -split ';') -notcontains $p)) {
        $env:Path = "$p;$env:Path"
    }
}

if (-not (Test-Path $qtRoot)) { throw "Qt not found at $qtRoot." }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsRoot = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Workload.VCTools `
    -property installationPath
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$tmp = New-TemporaryFile
cmd /c "`"$vcvars`" >nul && set" | Out-File -FilePath $tmp -Encoding ascii
foreach ($line in Get-Content $tmp) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}
Remove-Item $tmp

$env:Path = "$qtRoot\bin;$env:Path"
$env:CMAKE_PREFIX_PATH = $qtRoot

if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Force -Path $buildDir | Out-Null }

& cmake -S $repoRoot -B $buildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_PREFIX_PATH="$qtRoot"
if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

& cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

Push-Location $buildDir
try {
    & ctest --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
