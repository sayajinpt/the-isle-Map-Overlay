# The Isle Companion (v2)

Windows C++ / Qt 6 map companion for The Isle: Evrima.

## Build

```bat
cd v2
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Shared assets live one level up from `v2/`:

- `../map`
- `../data`
- `../tools`
- `../core/windows_ocr.ps1`

## Features

- Mini Map main window + Options full map
- Clipboard / OCR / live-map bridge (Bosch or Voice Island)
- Waypoints, breadcrumbs, prime run, zone layers
- System tray, calibration editor, userscript helpers
