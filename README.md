# lg_apa102

ESP32-S2 Mini firmware inspired by `legacy-bridge`:

- main firmware on OTA_0
- recovery firmware in factory partition
- signed release packages
- configurable GPIO in the UI
- update source pointed at a GitHub Pages or release directory

## Hardware

- Wemos S2 Mini / ESP32-S2
- APA102-style output is wired through configurable DATA / CLOCK / OE / POWER pins

## Default pins

- DATA: GPIO11
- CLOCK: GPIO7
- OE: GPIO18
- POWER: GPIO9

GPIO46 is intentionally rejected.

## Build

```powershell
pio run -e lolin_s2_mini
pio run -e lolin_s2_mini_recovery
```

## Release flow

`scripts/build_release.ps1` builds both binaries, signs them, and writes:

- `release.txt`
- `firmware.bin`
- `firmware.sig`
- `recovery.bin`
- `recovery.sig`

Point `packBaseUrl` at the directory containing those files.

## Notes

- Transport can be HTTPS with `setInsecure()` because payload integrity is checked by RSA/SHA-256 signatures.
- The public key is embedded in `include/lb_public_key.h`.
- The private key stays in `keys/release_private.pem` and is ignored by Git.
