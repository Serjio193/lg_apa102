param(
  [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$bin = Join-Path $root ".pio\\build\\lolin_s2_mini_recovery\\firmware.bin"

if (!(Test-Path $bin)) {
  throw "Recovery binary not found. Build the project first."
}

pio run -d $root -e lolin_s2_mini_recovery -t upload --upload-port $Port
