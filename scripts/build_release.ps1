param(
  [string]$Environment = "lolin_s2_mini",
  [string]$Version = "dev",
  [string]$ReleaseDir = "$(Join-Path $PSScriptRoot '..\\dist\\latest')"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$privateKey = Join-Path $root "keys\\release_private.pem"
$signTool = Join-Path $root "tools\\sign_artifact.py"
$versionHeader = Join-Path $root "include\\lb_version.h"

$python = $env:PYTHON
if (-not $python) {
  $candidates = @(
    "C:\\Users\\serji\\AppData\\Local\\Programs\\Python\\Python313\\python.exe",
    "python3",
    "python"
  )
  foreach ($candidate in $candidates) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) {
      $python = $cmd.Source
      break
    }
  }
}
if (-not $python) { throw "Python not found" }

New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null

@"
#pragma once

#define LB_FIRMWARE_VERSION "$Version"
"@ | Set-Content -Encoding ASCII $versionHeader

& $python -m platformio run -d $root -e $Environment
& $python -m platformio run -d $root -e "${Environment}_recovery"

$firmware = Join-Path $root ".pio\\build\\$Environment\\firmware.bin"
$recovery = Join-Path $root ".pio\\build\\${Environment}_recovery\\firmware.bin"

if (!(Test-Path $firmware)) { throw "Missing firmware: $firmware" }
if (!(Test-Path $recovery)) { throw "Missing recovery: $recovery" }
if (!(Test-Path $privateKey)) { throw "Missing private key: $privateKey" }

Copy-Item $firmware (Join-Path $ReleaseDir "firmware.bin") -Force
Copy-Item $recovery (Join-Path $ReleaseDir "recovery.bin") -Force
& $python $signTool (Join-Path $ReleaseDir "firmware.bin") $privateKey (Join-Path $ReleaseDir "firmware.sig")
& $python $signTool (Join-Path $ReleaseDir "recovery.bin") $privateKey (Join-Path $ReleaseDir "recovery.sig")

@"
version=$Version
firmware=firmware.bin
firmware_sig=firmware.sig
recovery=recovery.bin
recovery_sig=recovery.sig
"@ | Set-Content -Encoding ASCII (Join-Path $ReleaseDir "release.txt")

Write-Host "Release prepared in $ReleaseDir"
