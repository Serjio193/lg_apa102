param(
  [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$bin = Join-Path $root ".pio\\build\\lolin_s2_mini_recovery\\firmware.bin"
$esptool = Join-Path $env:USERPROFILE ".platformio\\packages\\tool-esptoolpy\\esptool.py"
$python = "C:\\Users\\serji\\AppData\\Local\\Programs\\Python\\Python313\\python.exe"

if (!(Test-Path $bin)) {
  throw "Recovery binary not found. Build the project first."
}
if (!(Test-Path $esptool)) { throw "esptool.py not found: $esptool" }
if (!(Test-Path $python)) { throw "Python not found: $python" }

& $python $esptool --chip esp32s2 --port $Port --baud 921600 write_flash 0x2A0000 $bin
if ($LASTEXITCODE -ne 0) { throw "Recovery flash failed: $LASTEXITCODE" }
