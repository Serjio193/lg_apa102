param(
  [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")

pio run -d $root -e lolin_s2_mini -t upload --upload-port $Port
