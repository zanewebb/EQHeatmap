<# 
Rebuild & install EQHeatmap VST3 (Windows, PowerShell)
- Cleans build/, configures VS2022 x64, builds Release
- Copies bundle to %LOCALAPPDATA%\Programs\Common\VST3 (no admin)
- Optional: -CopyToSystem also copies to C:\Program Files\Common Files\VST3 (self-elevates)
- Optional: -KillDAWs stops REAPER/Ableton & common helper processes to avoid locks
#>

[CmdletBinding()]
param(
  [ValidateSet('Release','Debug')] [string]$Config = 'Release',
  [string]$Generator = 'Visual Studio 17 2022',
  [ValidateSet('x64','Win32','ARM64')] [string]$Arch = 'x64',
  [switch]$CopyToSystem,
  [switch]$KillDAWs
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Header($t){ Write-Host "`n=== $t ===" -ForegroundColor Cyan }
function Is-Admin {
  $id = [Security.Principal.WindowsIdentity]::GetCurrent()
  $p  = New-Object Security.Principal.WindowsPrincipal($id)
  return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
function Ensure-Dir($p){ if(-not (Test-Path -LiteralPath $p)){ New-Item -ItemType Directory -Force -Path $p | Out-Null } }

# Kill likely lockers
function Kill-Lockers {
  Write-Header "Stopping DAWs/hosts to prevent file locks"
  $names = @(
    'reaper','reaper_host64','reaper_smp',
    'Ableton Index','Ableton Web Connector','Ableton Song Statistics',
    'EQHeatmap_vst3_helper','juce_VST3PluginHost','PluginScanner'
  )
  foreach($n in $names){
    Get-Process -Name $n -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  }
}

# Clear attributes on all files under a path
function Clear-Attrs($path){
  if(Test-Path -LiteralPath $path){
    Get-ChildItem -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue |
      ForEach-Object {
        try { $_.Attributes = 'Archive' } catch {}
      }
  }
}

# Robust directory removal with retries
function Remove-DirUnlocked($path){
  if(-not (Test-Path -LiteralPath $path)) { return }
  Clear-Attrs $path
  $tries = 6
  for($i=1; $i -le $tries; $i++){
    try {
      Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
      return
    } catch {
      if($i -eq 1){ Write-Host "Delete failed (likely locked). Retrying..." -ForegroundColor Yellow }
      Start-Sleep -Milliseconds (300 * $i)
      if($i -eq 3 -and $KillDAWs){ Kill-Lockers }
    }
  }
  throw "Failed to remove: $path (still locked)"
}

# 0) Optional: kill DAWs up front
if ($KillDAWs) { Kill-Lockers }

# 1) Paths
$root      = (Resolve-Path .).Path
$buildDir  = Join-Path $root 'build'
$userVst3  = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
$sysVst3   = 'C:\Program Files\Common Files\VST3'
$bundleRel = Join-Path $buildDir ("EQHeatmap_artefacts\{0}\VST3\EQHeatmap.vst3" -f $Config)
$bundleWin = Join-Path $bundleRel 'Contents\x86_64-win\EQHeatmap.vst3'

Write-Header "Clean build dir"
if (Test-Path -LiteralPath $buildDir) { Remove-DirUnlocked $buildDir }

# 2) Configure
Write-Header "CMake configure ($Generator / $Arch)"
$copyDirForCMake = ($userVst3 -replace '\\','/')
$cmakeConfigure = @(
  '-S','.', '-B','build',
  '-G', $Generator, '-A', $Arch,
  "-DVST3_COPY_DIR=$copyDirForCMake"
)
& cmake @cmakeConfigure

# 3) Build VST3
Write-Header "CMake build (target EQHeatmap_VST3, $Config)"
& cmake --build $buildDir --config $Config --target EQHeatmap_VST3

# 4) Locate bundle
Write-Header "Locating built bundle"
if (-not (Test-Path -LiteralPath $bundleWin)) {
  $candidate = Get-ChildItem -Recurse $buildDir -Directory -Filter '*.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($candidate) { $bundleRel = $candidate.FullName; $bundleWin = Join-Path $bundleRel 'Contents\x86_64-win\EQHeatmap.vst3' }
}
if (-not (Test-Path -LiteralPath $bundleWin)) {
  throw "Plugin binary not found. Expected: $bundleWin"
}
Write-Host "Found binary: $bundleWin"

# 5) Copy to per-user VST3
Write-Header "Copying to per-user VST3 ($userVst3)"
Ensure-Dir $userVst3
$dstBundle = Join-Path $userVst3 'EQHeatmap.vst3'
if (Test-Path -LiteralPath $dstBundle) { Remove-DirUnlocked $dstBundle }
Copy-Item -Recurse -Force -LiteralPath $bundleRel -Destination $userVst3
Write-Host "Installed: $dstBundle" -ForegroundColor Green

# 6) Optionally copy to system VST3 (self-elevate if needed)
if ($CopyToSystem) {
  if (-not (Is-Admin)) {
    Write-Header "Elevating to Administrator to copy to system VST3"
    $ps = "$PSCommandPath"
    $args = @()
    if ($Config)     { $args += "-Config `"$Config`"" }
    if ($Generator)  { $args += "-Generator `"$Generator`"" }
    if ($Arch)       { $args += "-Arch `"$Arch`"" }
    if ($KillDAWs)   { $args += "-KillDAWs" }
    $args += "-CopyToSystem"
    Start-Process -Verb RunAs -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy Bypass -NoProfile -File `"$ps`" $($args -join ' ')" | Out-Null
    Write-Host "Spawned elevated copy; this window can be closed." -ForegroundColor Yellow
    exit 0
  }

  Write-Header "Copying to system VST3 ($sysVst3)"
  Ensure-Dir $sysVst3
  $sysDst = Join-Path $sysVst3 'EQHeatmap.vst3'
  if (Test-Path -LiteralPath $sysDst) { Remove-DirUnlocked $sysDst }
  Copy-Item -Recurse -Force -LiteralPath $bundleRel -Destination $sysVst3
  Write-Host "Installed: $sysDst" -ForegroundColor Green
}

Write-Header "Done"
Write-Host "Tip: In REAPER -> Preferences -> Plug-ins -> VST -> Clear cache/re-scan" -ForegroundColor DarkGray
Write-Host "     In Ableton -> Preferences -> Plug-Ins -> Rescan (hold Alt for forced rescan)" -ForegroundColor DarkGray
