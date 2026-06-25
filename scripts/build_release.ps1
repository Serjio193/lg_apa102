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
$distRoot = Split-Path -Parent $ReleaseDir
$releaseLicenses = Join-Path $ReleaseDir "licenses"

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
New-Item -ItemType Directory -Force -Path $releaseLicenses | Out-Null

@"
#pragma once

#define LB_FIRMWARE_VERSION "$Version"
"@ | Set-Content -Encoding ASCII $versionHeader

& $python -m platformio run -d $root -e $Environment
if ($LASTEXITCODE -ne 0) { throw "Main firmware build failed: $LASTEXITCODE" }
& $python -m platformio run -d $root -e "${Environment}_recovery"
if ($LASTEXITCODE -ne 0) { throw "Recovery firmware build failed: $LASTEXITCODE" }

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
license=LICENSE.txt
third_party_notices=THIRD_PARTY_NOTICES.md
"@ | Set-Content -Encoding ASCII (Join-Path $ReleaseDir "release.txt")

Copy-Item (Join-Path $root "LICENSE") (Join-Path $ReleaseDir "LICENSE.txt") -Force
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") (Join-Path $ReleaseDir "THIRD_PARTY_NOTICES.md") -Force
Copy-Item (Join-Path $root "licenses\\*") $releaseLicenses -Force
@"
<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>lg_apa102 licenses</title></head>
<body style="font-family:system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#070b12;color:#e7edf8">
<h1>Third-party license texts</h1>
<ul>
<li><a href="HyperSerialWLED-MIT.txt">HyperSerialWLED MIT</a></li>
<li><a href="WLED-MIT.txt">WLED MIT</a></li>
<li><a href="React-MIT.txt">React MIT</a></li>
<li><a href="esbuild-MIT.txt">esbuild MIT</a></li>
<li><a href="LGPL-2.1.txt">Arduino-ESP32 LGPL-2.1</a></li>
<li><a href="Apache-2.0.txt">ESP-IDF and Mbed TLS Apache-2.0</a></li>
<li><a href="PyCryptodome-LICENSE.rst">PyCryptodome licenses</a></li>
</ul>
</body></html>
"@ | Set-Content -Encoding UTF8 (Join-Path $releaseLicenses "index.html")

$latestIndex = @"
<!doctype html>
<html lang="ru">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>lg_apa102 $Version</title></head>
<body style="font-family:system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#070b12;color:#e7edf8">
<h1>lg_apa102 $Version</h1>
<p>Подписанный пакет обновления для Wemos S2 Mini.</p>
<ul>
<li><a href="release.txt">release.txt</a></li>
<li><a href="firmware.bin">firmware.bin</a></li>
<li><a href="firmware.sig">firmware.sig</a></li>
<li><a href="recovery.bin">recovery.bin</a></li>
<li><a href="recovery.sig">recovery.sig</a></li>
<li><a href="LICENSE.txt">LICENSE.txt</a></li>
<li><a href="THIRD_PARTY_NOTICES.md">THIRD_PARTY_NOTICES.md</a></li>
<li><a href="licenses/">Third-party license texts</a></li>
</ul>
</body></html>
"@
$latestIndex | Set-Content -Encoding UTF8 (Join-Path $ReleaseDir "index.html")

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
@"
<!doctype html>
<html lang="ru">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>lg_apa102 releases</title></head>
<body style="font-family:system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#070b12;color:#e7edf8">
<h1>lg_apa102</h1>
<p><a href="latest/">Открыть последний релиз</a></p>
</body></html>
"@ | Set-Content -Encoding UTF8 (Join-Path $distRoot "index.html")

Write-Host "Release prepared in $ReleaseDir"
