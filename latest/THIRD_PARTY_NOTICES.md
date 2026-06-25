# Third-party notices

`lg_apa102` contains or is built with the components listed below. The project
license in `LICENSE` applies only to original project code and does not replace
the licenses of these components.

## AWA serial protocol implementation

The AWA frame parser in `src/main.cpp` is adapted from:

- HyperSerialWLED: <https://github.com/awawa-dev/HyperSerialWLED>
- Original AWA implementation commit:
  <https://github.com/awawa-dev/HyperSerialWLED/commit/608da5a0e4048332983b6f99351860dd3f6d212d>
- HyperSerialEsp8266, the base AWA project:
  <https://github.com/awawa-dev/HyperSerialEsp8266>

Copyright (c) 2020-2025 awawa-dev.

HyperSerialWLED is derived from WLED:
<https://github.com/Aircoookie/WLED>

Copyright (c) 2016 Christian Schwinne.

These components are licensed under the MIT License. The complete MIT terms
and applicable copyright notices are included in:

- `licenses/HyperSerialWLED-MIT.txt`
- `licenses/WLED-MIT.txt`

## Embedded web interface

The generated web interface embedded in the firmware contains:

- React 18.3.1
- React DOM 18.3.1
- Scheduler 0.23.2

Copyright (c) Facebook, Inc. and its affiliates.

These components are licensed under the MIT License. See
`licenses/React-MIT.txt`.

## ESP32 firmware framework

The firmware is linked with:

- Arduino core for ESP32 2.0.6:
  <https://github.com/espressif/arduino-esp32/tree/2.0.6>
- ESP-IDF components supplied by that Arduino core:
  <https://github.com/espressif/esp-idf>
- Mbed TLS supplied by ESP-IDF:
  <https://github.com/Mbed-TLS/mbedtls>

Arduino-ESP32 is licensed under LGPL-2.1-or-later. See
`licenses/LGPL-2.1.txt`.

ESP-IDF and Mbed TLS are primarily licensed under Apache-2.0. See
`licenses/Apache-2.0.txt`. Individual ESP-IDF components can contain
additional permissive notices; their source and notices are available from
the upstream ESP-IDF repository.

The complete project source, build configuration, and scripts required to
rebuild the firmware are available at:
<https://github.com/Serjio193/lg_apa102>

## Build-time tools

The following tools are used to create release artifacts but are not embedded
as executable code in the firmware:

- esbuild 0.25.12, MIT License, copyright (c) 2020 Evan Wallace.
  See `licenses/esbuild-MIT.txt`.
- PyCryptodome, used by `tools/sign_artifact.py`.
  See `licenses/PyCryptodome-LICENSE.rst`.

## User-owned legacy source

The signed Recovery/OTA design was adapted from the repository owner's
`Serjio193/legacy-bridge` project. It is not listed as a third-party component
because both repositories are owned by the same author and no external
license notice is present in that source repository.
