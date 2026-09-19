<#
.SYNOPSIS
Builds and runs the Universal Hardware Session Bridge developer prototype on Windows.

.DESCRIPTION
This script never writes into the selected source project folder. It creates the
BridgeOutput folder beside the source project by default. Use a separate destination
if you prefer. Install a supported Visual Studio C++ desktop workload and CMake
before running this script. Visual Studio 2026 is preferred; 2022 is supported as
a fallback for contributors who have not upgraded yet.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path $_ -PathType Container })]
    [string]$ProjectFolder,

    [Parameter(Mandatory = $true)]
    [ValidateSet('cubase', 'reason')]
    [string]$Daw,

    [ValidateSet('windows', 'macos', 'linux', 'android', 'chromeos', 'ipados', 'ios')]
    [string]$TargetOs = 'windows',

    [string]$OutputFolder = (Join-Path (Split-Path -Parent $ProjectFolder) 'BridgeOutput'),

    [switch]$NoBackup,
    [switch]$NoCopyAssets
)

$ErrorActionPreference = 'Stop'
$RepositoryRoot = Split-Path -Parent $PSScriptRoot
$BuildFolder = Join-Path $RepositoryRoot 'build\windows-msvc'

if ((Resolve-Path $ProjectFolder).Path -eq (Resolve-Path $OutputFolder -ErrorAction SilentlyContinue).Path) {
    throw 'OutputFolder must be different from ProjectFolder.'
}

$CMakeHelp = (cmake --help | Out-String)
$Generator = @('Visual Studio 18 2026', 'Visual Studio 17 2022') |
    Where-Object { $CMakeHelp.Contains($_) } |
    Select-Object -First 1

if (-not $Generator) {
    throw 'No supported Visual Studio C++ generator was found. Install the Desktop development with C++ workload.'
}

Write-Host "Configuring Universal Bridge with $Generator (x64)." -ForegroundColor Cyan
cmake -S $RepositoryRoot -B $BuildFolder -G $Generator -A x64 -D BUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

cmake --build $BuildFolder --config Release
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}

$Executable = Join-Path $BuildFolder 'bin\Release\ubridge.exe'
if (-not (Test-Path $Executable)) {
    throw "Build completed but executable was not found: $Executable"
}

$Arguments = @('preflight', '--project', $ProjectFolder, '--daw', $Daw, '--target-os', $TargetOs, '--output', $OutputFolder)
if ($NoBackup) { $Arguments += '--no-backup' }
if ($NoCopyAssets) { $Arguments += '--no-copy-assets' }

& $Executable @Arguments
if ($LASTEXITCODE -ne 0) {
    throw "Universal Bridge preflight failed with exit code $LASTEXITCODE."
}

Write-Host "`nUniversal Bridge output: $OutputFolder" -ForegroundColor Green
Write-Host 'Open preflight-report.md first, then follow Exchange/IMPORT_*.md.' -ForegroundColor Green
