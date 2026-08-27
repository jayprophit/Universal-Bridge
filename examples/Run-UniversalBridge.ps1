<#
.SYNOPSIS
Builds and runs the Universal Hardware Session Bridge developer prototype on Windows.

.DESCRIPTION
This script never writes into the selected source project folder. It creates the
BridgeOutput folder beside the source project by default. Use a separate destination
if you prefer. Install Visual Studio 2022 Build Tools with the C++ desktop workload
and CMake before running this script.
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
$BuildFolder = Join-Path $RepositoryRoot 'build-windows'

if ((Resolve-Path $ProjectFolder).Path -eq (Resolve-Path $OutputFolder -ErrorAction SilentlyContinue).Path) {
    throw 'OutputFolder must be different from ProjectFolder.'
}

cmake -S $RepositoryRoot -B $BuildFolder -G 'Visual Studio 17 2022' -A x64
cmake --build $BuildFolder --config Release

$Executable = Join-Path $BuildFolder 'Release\ubridge.exe'
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
