#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('all', 'server', 'client')]
    [string]$Target = 'all',

    [Parameter(Position = 1)]
    [ValidateSet('debug', 'release')]
    [string]$Config = 'debug',

    [Parameter(Position = 2)]
    [ValidateSet('qmake', 'build', 'rebuild', 'clean')]
    [string]$Action = 'build',

    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'

function Get-ToolPath([string]$EnvName, [string]$DefaultPath)
{
    $value = [Environment]::GetEnvironmentVariable($EnvName)
    if (![string]::IsNullOrWhiteSpace($value)) {
        return $value
    }
    return $DefaultPath
}

$qmake = Get-ToolPath 'QT_QMAKE' 'D:\DEV\QT\6.9.2\mingw_64\bin\qmake.exe'
$make = Get-ToolPath 'QT_MAKE' 'D:\DEV\QT\Tools\mingw1310_64\bin\mingw32-make.exe'

if (!(Test-Path $qmake)) {
    throw "qmake not found: $qmake (set env QT_QMAKE to override)"
}
if (!(Test-Path $make)) {
    throw "make not found: $make (set env QT_MAKE to override)"
}

# Prefer Qt's bundled MinGW + Qt bin for consistent headers/libs.
$env:PATH = (Split-Path -Parent $make) + ';' + (Split-Path -Parent $qmake) + ';' + $env:PATH

try { chcp 65001 | Out-Null } catch {}

$root = Resolve-Path (Join-Path $PSScriptRoot '..')

function Invoke-Checked([string]$FilePath, [string[]]$Arguments)
{
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Get-BuildDir([string]$name)
{
    return Join-Path $root ("build\\vscode-{0}-{1}" -f $name, $Config)
}

function Ensure-Dir([string]$path)
{
    if (!(Test-Path $path)) {
        New-Item -ItemType Directory -Path $path | Out-Null
    }
}

function Configure-Project([string]$name, [string]$proRelFromRoot)
{
    $buildDir = Get-BuildDir $name
    Ensure-Dir $buildDir

    Push-Location $buildDir
    try {
        $proRelFromBuild = Join-Path '..\..' $proRelFromRoot
        $cfg = "CONFIG+=$Config"
        Invoke-Checked $qmake @($proRelFromBuild, '-spec', 'win32-g++', $cfg, 'CONFIG+=qml_debug')
    } finally {
        Pop-Location
    }
}

function Clean-Project([string]$name)
{
    $buildDir = Get-BuildDir $name
    if (!(Test-Path (Join-Path $buildDir 'Makefile'))) {
        return
    }
    Push-Location $buildDir
    try {
        Invoke-Checked $make @('clean')
    } finally {
        Pop-Location
    }
}

function Build-Project([string]$name, [string]$proRelFromRoot)
{
    $buildDir = Get-BuildDir $name
    Ensure-Dir $buildDir

    $makefile = Join-Path $buildDir 'Makefile'
    if (!(Test-Path $makefile)) {
        Configure-Project $name $proRelFromRoot
    }

    Push-Location $buildDir
    try {
        Invoke-Checked $make @("-j$Jobs")
    } finally {
        Pop-Location
    }
}

function Rebuild-Project([string]$name, [string]$proRelFromRoot)
{
    Configure-Project $name $proRelFromRoot

    $buildDir = Get-BuildDir $name
    Push-Location $buildDir
    try {
        Invoke-Checked $make @('clean')
        Invoke-Checked $make @("-j$Jobs")
    } finally {
        Pop-Location
    }
}

function Configure-Targets()
{
    if ($Target -eq 'server' -or $Target -eq 'all') { Configure-Project 'server' 'server\server.pro' }
    if ($Target -eq 'client' -or $Target -eq 'all') { Configure-Project 'client' 'client\client.pro' }
}

function Build-Targets()
{
    if ($Target -eq 'server' -or $Target -eq 'all') { Build-Project 'server' 'server\server.pro' }
    if ($Target -eq 'client' -or $Target -eq 'all') { Build-Project 'client' 'client\client.pro' }
}

function Clean-Targets()
{
    if ($Target -eq 'server' -or $Target -eq 'all') { Clean-Project 'server' }
    if ($Target -eq 'client' -or $Target -eq 'all') { Clean-Project 'client' }
}

function Rebuild-Targets()
{
    if ($Target -eq 'server' -or $Target -eq 'all') { Rebuild-Project 'server' 'server\server.pro' }
    if ($Target -eq 'client' -or $Target -eq 'all') { Rebuild-Project 'client' 'client\client.pro' }
}

switch ($Action) {
    'qmake' { Configure-Targets; break }
    'clean' { Clean-Targets; break }
    'build' { Build-Targets; break }
    'rebuild' { Rebuild-Targets; break }
}
