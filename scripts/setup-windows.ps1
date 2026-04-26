# Dental Patients - one-shot Windows dev environment setup.
# Run from an elevated PowerShell on Windows 10/11.
# Total download: ~5 GB. Time: ~30-45 min depending on bandwidth.

$ErrorActionPreference = 'Stop'
$logRoot = Join-Path $PSScriptRoot '..\build\setup-logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$qtVersion = '6.10.3'
$qtArch = 'win64_msvc2022_64'

function Add-KnownToolPaths {
    $paths = @(
        (Join-Path $env:ProgramFiles 'CMake\bin'),
        (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'),
        (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6')
    )
    foreach ($p in $paths) {
        if ((Test-Path $p) -and (($env:Path -split ';') -notcontains $p)) {
            $env:Path = "$p;$env:Path"
        }
    }
}

function Resolve-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'),
        (Join-Path $env:ProgramFiles 'Python312\python.exe')
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    $cmd = Get-Command python.exe -ErrorAction Stop
    if ($cmd.Source -like '*\WindowsApps\python.exe') {
        throw 'Python resolves to the Microsoft Store alias. Re-run winget install Python.Python.3.12 or restart PowerShell.'
    }
    return $cmd.Source
}

function Step($name, $block) {
    $t0 = Get-Date
    Write-Host "==> $name"
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $block
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldPreference
    }
    if ($exitCode -ne 0) {
        throw "$name failed with exit code $exitCode"
    }
    $dt = (Get-Date) - $t0
    Write-Host "    done in $([math]::Round($dt.TotalSeconds,1))s"
}

# 1. Light installs via winget (CMake, Ninja, Python, Inno Setup, Git LFS optional).
$light = @(
    'Kitware.CMake',
    'Ninja-build.Ninja',
    'Python.Python.3.12',
    'JRSoftware.InnoSetup'
)
foreach ($id in $light) {
    Step "winget install $id" {
        winget install --id $id --silent --accept-source-agreements --accept-package-agreements 2>&1 |
            Tee-Object -FilePath (Join-Path $logRoot "$id.log")
    }
    Add-KnownToolPaths
}

# 2. VS 2022 Build Tools - C++ workload + Win11 SDK only (skip the rest of VS).
Step 'winget install Microsoft.VisualStudio.2022.BuildTools (C++ workload)' {
    $vsArgs = '--quiet --wait --norestart --nocache ' +
              '--add Microsoft.VisualStudio.Workload.VCTools ' +
              '--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ' +
              '--add Microsoft.VisualStudio.Component.Windows11SDK.22621 ' +
              '--includeRecommended'
    winget install --id Microsoft.VisualStudio.2022.BuildTools --silent `
        --accept-source-agreements --accept-package-agreements `
        --override $vsArgs 2>&1 | Tee-Object -FilePath (Join-Path $logRoot 'vs-buildtools.log')
}

# 3. Qt 6 via aqtinstall - faster + no Qt account required vs the official online installer.
Step 'pip install aqtinstall' {
    Add-KnownToolPaths
    $python = Resolve-Python
    & $python -m pip install --upgrade pip aqtinstall |
        Tee-Object -FilePath (Join-Path $logRoot 'aqtinstall-pip.log')
}
Step "aqt install-qt $qtVersion $qtArch -> C:\Qt" {
    Add-KnownToolPaths
    $python = Resolve-Python
    & $python -m aqt install-qt windows desktop $qtVersion $qtArch -O C:\Qt |
        Tee-Object -FilePath (Join-Path $logRoot 'qt.log')
}

Write-Host ''
Write-Host 'Setup complete. Build with:'
Write-Host '  scripts\build-release.ps1'
