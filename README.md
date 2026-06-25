# lg_apa102

ESP32-S2 Mini firmware inspired by `legacy-bridge`:

- main firmware on OTA_0
- recovery firmware in factory partition
- signed release packages
- configurable GPIO in the UI
- configurable signed-update source
- optional PIN protection for control and recovery APIs

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

The canonical release directory is:

`https://serjio193.github.io/lg_apa102/latest/`

`packBaseUrl` can be changed in Settings. It must point at an HTTP(S)
directory containing `release.txt` and the signed artifacts.

GitHub Actions installs both Python and Node dependencies, builds the React UI
from source, creates `dist/latest`, and publishes the whole `dist` directory.

## Web API protection

The device starts with API protection disabled so an existing installation is
not locked out. Configure a 4-16 character PIN in Settings. After that, all
state-changing endpoints, Wi-Fi scans, logs, and recovery actions require the
`X-API-PIN` header. The browser stores the PIN only in `sessionStorage`.

## Notes

- Transport can be HTTPS with `setInsecure()` because payload integrity is checked by RSA/SHA-256 signatures.
- The public key is embedded in `include/lb_public_key.h`.
- The private key stays in `keys/release_private.pem` and is ignored by Git.
