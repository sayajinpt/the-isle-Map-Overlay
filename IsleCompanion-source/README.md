# The Isle Companion

Local Windows map companion for **The Isle: Evrima** (C++ / Qt 6).

Tracks your position from:

- **Clipboard** (in-game Copy Location)
- **Live map bridge** (Bosch Island or Voice Island via Tampermonkey → localhost)
- **Optional OCR** (Windows)

No game-memory reading and no anti-cheat bypass.

## Requirements

- Windows 10/11 x64
- [CMake](https://cmake.org/) 3.21+
- [Qt 6.5+](https://www.qt.io/download-qt-installer) (Widgets, Network, Gui) — MSVC 64-bit kit
- Visual Studio 2022 Build Tools (MSVC)

## Build

```bat
cd v2
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Point `CMAKE_PREFIX_PATH` at your Qt kit. Run:

`v2\build\Release\IsleCompanionV2.exe`

Keep the repo `map\`, `data\`, `tools\`, and `core\` folders available (the app resolves them from the project root).

## Portable package for other players

Ship a Release zip with the exe, Qt DLLs (`windeployqt`), plus `map\`, `data\`, `tools\`, and `core\windows_ocr.ps1` next to the exe. Do not commit that zip into git — attach it to a GitHub **Release**.

## Live map scripts

| Server | Userscript |
| --- | --- |
| Bosch Island | `tools/bosch_island_bridge.user.js` |
| Voice Island | `tools/voice_island_bridge.user.js` |

Install with Tampermonkey, enable **Live map bridge** in Options, and open the matching livemap page.

## Layout

```
map/                 Gateway map + calibration
data/                Zones / POIs
tools/               Tampermonkey bridge scripts
core/windows_ocr.ps1 OCR helper (Windows)
v2/                  C++ / Qt application
  CMakeLists.txt
  src/
```
