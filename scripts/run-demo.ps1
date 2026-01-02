#!/usr/bin/env pwsh

[CmdletBinding()]
param(
    [int]$Clients = 2,
    [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$buildRoot = Join-Path $root 'build'

function Find-LatestExe([string]$searchRoot, [string]$fileName)
{
    if (!(Test-Path $searchRoot)) {
        return $null
    }

    return Get-ChildItem -Path $searchRoot -Recurse -File -Filter $fileName -ErrorAction SilentlyContinue |
        Sort-Object -Property LastWriteTime -Descending |
        Select-Object -First 1
}

function Find-BuildPair()
{
    if (!(Test-Path $buildRoot)) {
        return $null
    }

    $pairs = @()
    foreach ($dir in (Get-ChildItem -Path $buildRoot -Directory -ErrorAction SilentlyContinue)) {
        $server = Find-LatestExe $dir.FullName 'server.exe'
        $client = Find-LatestExe $dir.FullName 'client.exe'
        if (-not $server -or -not $client) {
            continue
        }

        $stamp = $server.LastWriteTime
        if ($client.LastWriteTime -gt $stamp) {
            $stamp = $client.LastWriteTime
        }

        $pairs += [pscustomobject]@{
            BuildDir = $dir.FullName
            Server   = $server
            Client   = $client
            Stamp    = $stamp
        }
    }

    return $pairs | Sort-Object -Property Stamp -Descending | Select-Object -First 1
}

function Find-QmakePath([string]$startDir)
{
    $dir = $startDir
    while ($dir -and (Test-Path $dir)) {
        $makefile = Join-Path $dir 'Makefile'
        if (Test-Path $makefile) {
            $text = Get-Content -Path $makefile -Raw -ErrorAction SilentlyContinue
            if ($text -and ($text -match '(?m)^QMAKE\s*=\s*(.+)$')) {
                $qmakePath = $Matches[1].Trim()
                if (Test-Path $qmakePath) {
                    return $qmakePath
                }
            }
        }

        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) {
            break
        }
        $dir = $parent
    }

    $cmd = Get-Command qmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    return $null
}

function Find-MingwBin([string]$startDir, [string]$qmakePath)
{
    $dir = $startDir
    while ($dir -and (Test-Path $dir)) {
        $stash = Join-Path $dir '.qmake.stash'
        if (Test-Path $stash) {
            $text = Get-Content -Path $stash -Raw -ErrorAction SilentlyContinue
            if ($text -and ($text -match '([A-Za-z]:[\\/][^\s]*?[\\/]Tools[\\/]mingw[^\s\\/]+)')) {
                $mingwRoot = ($Matches[1] -replace '/', '\')
                $mingwBin = Join-Path $mingwRoot 'bin'
                if (Test-Path $mingwBin) {
                    return $mingwBin
                }
            }
        }

        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) {
            break
        }
        $dir = $parent
    }

    if ($qmakePath -and (Test-Path $qmakePath)) {
        $qtRoot = $qmakePath
        for ($i = 0; $i -lt 4; $i++) {
            $qtRoot = Split-Path $qtRoot -Parent
        }

        $toolsDir = Join-Path $qtRoot 'Tools'
        if (Test-Path $toolsDir) {
            $bins = Get-ChildItem -Path $toolsDir -Directory -Filter 'mingw*' -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName 'bin' } |
                Where-Object { Test-Path $_ }

            foreach ($bin in $bins) {
                if (Test-Path (Join-Path $bin 'libstdc++-6.dll')) {
                    return $bin
                }
            }

            if ($bins) {
                return $bins | Select-Object -First 1
            }
        }
    }

    return $null
}

function Prepend-EnvPath([string[]]$paths)
{
    $existing = $env:PATH
    $add = @()
    foreach ($p in $paths) {
        if (-not $p) { continue }
        if (Test-Path $p) { $add += $p }
    }
    if ($add.Count -eq 0) {
        return
    }
    $env:PATH = ($add -join ';') + ';' + $existing
}

$serverExe = $null
$clientExe = $null

if ($BuildDir) {
    $buildDirPath = Resolve-Path $BuildDir -ErrorAction Stop
    $serverExe = Find-LatestExe $buildDirPath.Path 'server.exe'
    $clientExe = Find-LatestExe $buildDirPath.Path 'client.exe'
} else {
    $pair = Find-BuildPair
    if ($pair) {
        $serverExe = $pair.Server
        $clientExe = $pair.Client
    } else {
        $serverExe = Find-LatestExe $buildRoot 'server.exe'
        $clientExe = Find-LatestExe $buildRoot 'client.exe'
    }
}

if (-not $serverExe -or -not $clientExe) {
    Write-Host "ERROR: Cannot find executables. Build once in Qt Creator first (server + client)." -ForegroundColor Yellow
    Write-Host "Searched under: $buildRoot" -ForegroundColor Yellow
    if (-not $serverExe) { Write-Host "Missing: server.exe" -ForegroundColor Yellow }
    if (-not $clientExe) { Write-Host "Missing: client.exe" -ForegroundColor Yellow }
    exit 1
}

Write-Host "Server: $($serverExe.FullName)"
Write-Host "Client: $($clientExe.FullName)"

$qmakePath = Find-QmakePath $serverExe.DirectoryName
if ($qmakePath) {
    $qtBin = Split-Path $qmakePath -Parent
    $qtPlugins = $null
    try {
        $qtPlugins = (& $qmakePath -query QT_INSTALL_PLUGINS 2>$null)
    } catch {
        $qtPlugins = $null
    }

    if ($qtPlugins) {
        $qtPlugins = ($qtPlugins.Trim() -replace '/', '\')
        $env:QT_PLUGIN_PATH = $qtPlugins

        $qpa = Join-Path $qtPlugins 'platforms'
        if (Test-Path $qpa) {
            $env:QT_QPA_PLATFORM_PLUGIN_PATH = $qpa
        }
    }

    $mingwBin = Find-MingwBin $serverExe.DirectoryName $qmakePath
    Prepend-EnvPath @($qtBin, $mingwBin)

    Write-Host "Qt bin: $qtBin"
    if ($mingwBin) { Write-Host "MinGW bin: $mingwBin" }
} else {
    Write-Host "WARN: Cannot find qmake from Makefile. If you see missing DLLs, add Qt bin + MinGW bin to PATH." -ForegroundColor Yellow
}

$serverProc = Start-Process -FilePath $serverExe.FullName -WorkingDirectory $serverExe.DirectoryName -PassThru
Start-Sleep -Milliseconds 300
if ($serverProc -and $serverProc.HasExited) {
    Write-Host "ERROR: server.exe exited immediately (likely missing DLLs). Try rebuilding in Qt Creator and run again." -ForegroundColor Yellow
    exit 1
}

for ($i = 1; $i -le $Clients; $i++) {
    Start-Process -FilePath $clientExe.FullName -WorkingDirectory $clientExe.DirectoryName | Out-Null
    Start-Sleep -Milliseconds 150
}

Write-Host "Started: 1 server + $Clients clients."
Write-Host "Tip: Click 'Start' in the server window, then login in each client."
