<img width="314" height="330" alt="Captura de ecrã 2026-09-06 131624" src="https://github.com/user-attachments/assets/297531db-96a6-420d-bde5-e9b62656ebd0" />
# The Isle Companion

A local **Windows map companion** for [The Isle: Evrima] on the Gateway map.

** This app tryes to create some sort of Live map, avoiding the need for players to use TAB key to copy coords.

It sits beside the game as a Mini Map overlay and full map so you can track where you are, mark destinations, and see migrations, sanctuaries, patrol zones, water, and other useful layers.

-Live mode works well in Voice Island servers because theyr map updates more freequently than Bosch servers.

-In Bosch servers u might need to still use other technichs like copy-to-clipboard, between each Live update if u need position before next update.

-Bosch servers might seems frozen but is just because they update only on every 30 seconds.

-For the Prime run, the app keep track of were u were and change colours of the zones u were confirmed to be there.
   it only confirms u were there when the update comes from the live maps. 
   it does not confirm if u use copy to clipboard or ocr.
   like this we assure that the server confirmed our position.



## Quick start (players)

### Option A — Portable build (recommended)

1. Download **`IsleCompanionV2-Portable.zip`** from the latest [GitHub Release](../../releases).
2. Unzip the **whole folder** somewhere (keep every file next to the `.exe`).
3. Run `IsleCompanionV2.exe`.
4. Choose **Bosch** or **Voice Island**.
5. In Options, turn on the position sources you want (Clipboard and/or Live map bridge).

If Windows says a DLL is missing, install the [Visual C++ Redistributable (x64)](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

### Option B — Build from source

See [Build from source](#build-from-source) below.

---

## Live map setup (Bosch / Voice Island)
Only need to setup this once !

1. Install a userscript manager: **[Tampermonkey](https://www.tampermonkey.net/)** (Chrome / Edge / Firefox).
   - Go to the tampermonkey extention and select "manage extention", toogle ON "Allow user scripts"

2. In the companion:
   - Select the matching server at launch (or **Change Server…**)
   - Enable **Live map bridge** in Options  
   - Note the **Port** (often `8765` or `8770`) — the script auto-detects common ports

3. Install the script from this repo:
   - Bosch → [`tools/bosch_island_bridge.user.js`](tools/bosch_island_bridge.user.js)
   - Voice Island → [`tools/voice_island_bridge.user.js`](tools/voice_island_bridge.user.js)  
   - In app Options also has a Tab "livemap userscript", clicking it , exposes the scripts for user to copy/past
   
   
   Installing the script:
	- there are 2 scrips, u need to install both for both sources work(bosch and voice-island), do the script install process for both scripts !
	- go to your browser , select the tampermonkey extention u installed and select "create new script", copie the contents of the provided scripts into each new script and save it.
	-confirm the script toogle is ON(green) on tampermonkey.
	
	-Done
    
4. Just Open the livemap page and stay signed in if the site requires it, then just leave the page open in background and go to play. (https://bosch-island.com/map-tracker) or (https://voice-island.com/dashboard/livemap)
5. Confirm the small **badge** on the page shows the companion bridge as online / sending.
6.After this one time setup , everytime u play , u just need to open the webpage map, set to track and leave it in background while playing.

**Quick explain :
-install and configure tampermonkey extention
-intall the scripts into Tampermonkey
-reload the map webpage after scripts installed
-play the game.

**IMPORTANT INFO**
-while playing, after a while if the map stops updating(both bosch or voice island), just go back to the webpage map u have open in background and just reload the page. u can continue playing untill u need to do this again.
Its a tradeoff we have by having a some sort of Live map.  


**Voice Island tips**
- Updates roughly every few seconds (script polls about every 7s).
- If several players are online, type **your** map/player name in the badge and click **Track**.
- Badge **Mode** can switch coordinate modes if your pin ever looks wrong; default is raw API (`api`).

**Bosch tips**
- The script reads **Live Position** (Map X / Map Y) from the page.
- It soft-reloads the tracker tab periodically so Bosch live updates keep flowing.

---

---

## Features

### Map & navigation
- **Mini Map** as the main window (circle or square, always-on-top, resize/drag)
- **Full map** in Options (zoom, pan, layers)
- **Waypoints** — click the map to set a destination; distance and direction on the Mini Map
- **Breadcrumbs** — trail of where you have been
- **Nearest named POI** with distance and compass direction
- Toggleable layers: migrations, patrol zones, sanctuaries, water, updrafts, locations, food, AI, salt licks, spawns
- Per-zone filters and **prime-run** visit tracking (saved separately per live-map server)

### Position sources (all optional, all local)
| Source | How it works |
| --- | --- |
| **Clipboard** | Use in-game **Copy Location**; the companion reads the clipboard |
| **Live map bridge** | Tampermonkey script on Bosch or Voice Island posts your pin to `localhost` |
| **OCR** (Windows) | Optional: capture the Tab HUD with local Windows OCR |

### Live map servers
At launch you choose which community live map feeds the companion:

- **[Bosch Island](https://bosch-island.com/map-tracker)** — Map X / Map Y from the tracker page  
- **[Voice Island](https://voice-island.com/dashboard/livemap)** — positions from their livemap API / map (Discord login may be required)

Prime-run progress and filters are stored **per server**, so switching does not wipe the other profile.

---
<img width="2559" height="1439" alt="Captura de ecrã 2026-09-06 131554" src="https://github.com/user-attachments/assets/02c22c3a-a791-47b5-bfdd-16b7a632d189" />

## Security / what this is *not*

- Does **not** inject into the game  
- Does **not** read game process memory  
- Does **not** bypass Easy Anti-Cheat  
- Only uses clipboard, optional screen OCR, and an optional **localhost** bridge from your browser  

---


## Clipboard tracking

1. Enable **Clipboard** in Options (on by default).
2. In-game, open your location and use **Copy Location**.
3. The Mini Map should update to that position.

Works well as a backup when the live map is down, or on servers without a public livemap.

---

## OCR tracking (optional)

Windows only.

1. Options → **Set OCR Area…** and draw a rectangle over the Tab coordinate HUD.  
2. Enable **OCR / Automatic Tracking**.  
3. Hold **Tab** in-game so Lat / Long / Alt are visible.

OCR is best-effort; clipboard and live map are usually more reliable.

---

## Build from source

### Requirements
- Windows 10/11 x64  
- [CMake](https://cmake.org/) 3.21+  
- [Qt 6.5+](https://www.qt.io/download-qt-installer) — **MSVC 64-bit** kit (Widgets, Gui, Network)  
- Visual Studio 2022 **Build Tools** (MSVC)

### Configure & compile

```bat
cd v2
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```

Replace the Qt path with your installed kit.

Run:

```text
v2\build\Release\IsleCompanionV2.exe
```

The app expects these folders next to the project root (sibling of `v2/`):

| Path | Purpose |
| --- | --- |
| `map/` | Gateway map image + `calibration.json` |
| `data/` | Zones and POI JSON |
| `tools/` | Tampermonkey bridge scripts |
| `core/windows_ocr.ps1` | OCR helper script |

### Deploy for other PCs

From the Release build folder, run Qt’s `windeployqt` on the exe, then copy `map/`, `data/`, `tools/`, and `core/windows_ocr.ps1` next to it. Zip that folder and publish it as a **Release asset** (do not commit DLLs into git).

---

## Repository layout

```text
map/                  Gateway map + calibration
data/                 Layer / POI data
tools/                Live-map Tampermonkey scripts
core/windows_ocr.ps1  Windows OCR helper
v2/                   C++ / Qt application
  CMakeLists.txt
  src/
    app/              Startup, tray, wiring
    core/             Settings, coords, layers, prime run
    tracking/         Clipboard, OCR, live-map bridge
    ui/               Mini Map, Options, map view
```

---

## FAQ

**Does everyone need Qt installed?**  
No. Players using the portable Release zip only need the unzipped folder (and sometimes the VC++ redistributable). Qt is only required to *build* from source.

**Why “companion offline” on the userscript badge?**  
The companion is not running, Live map bridge is off, or the port in Options does not match. Enable the bridge and check the port shown in Options / on the badge.

**Can I use this on Linux?**  
The current **v2** app targets Windows (Qt + optional Windows OCR). Contributions welcome.

**Is this affiliated with The Isle, Bosch, or Voice Island?**  
No. Fan-made companion. Live maps are third-party community tools; their accuracy and availability can vary.

---

## License / credits

Map and layer data are community-oriented assets bundled for local use — see `map/SOURCE.md` where applicable.  
Respect The Isle’s and each live-map site’s terms when you play and when you scrape only your own browser session for the localhost bridge.
