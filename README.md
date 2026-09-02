# BDS-3 B2a GNSS SDR — C++ / Qt GUI

A Windows desktop app for post-processing a recorded BDS-3 B2a GNSS raw-IF
signal. It runs **acquisition → tracking (DLL/PLL) → navigation**, and shows
the results in an interactive multi-tab plot window:

- Acquisition peaks (per-PRN metric + threshold, channel table with per-PRN
  tracking popups)
- Tracking: I/Q scatter plot, navigation bits, raw/filtered PLL & DLL
  discriminators, correlation envelopes
- C/N0 signal quality
- Navigation position track (longitude/latitude)
- Sky view polar plot (per-PRN azimuth/elevation)

This file describes **everything needed to build and run the project on a
fresh system** after cloning from GitHub. No package managers (NuGet, vcpkg,
Conan, npm) are used — the only external dependency is Qt plus the bundled
QCustomPlot sources.

---

## 1. Requirements (software & versions)

| Requirement | Version | Notes |
|---|---|---|
| OS | **Windows 10 / 11 (64-bit)** | The project is built and tested on Windows only. |
| Visual Studio | **2022, 17.14 or newer** | Must include the **Desktop development with C++** workload. |
| MSVC toolset | **v145** (MSBuild 18.x) | Set in `BS-3-2a_GUI.vcxproj`. If your VS only has the older **v143** toolset, open the solution and use **Project → Retarget solution / Retarget projects** to v143 — it compiles fine. |
| Windows SDK | **10.0** (any recent) | Installed with the VS C++ workload. |
| Qt | **6.11.1 (MSVC 2022 64-bit)** | Install via the Qt Online Installer into `C:\Qt\6.11.1\msvc2022_64`. |
| Qt modules | `core`, `gui`, `widgets`, `printsupport` | Declared in the `.vcxproj` (`<QtModules>`); the installer enables these by default for a desktop Qt. |
| Qt Visual Studio Tools (extension) | **3.5.x** | Provides the Qt/MSBuild integration (`qt.props` / `qt.targets`, `moc`/`rcc`/`uic` steps) the project needs during `msbuild`. Install from Visual Studio → **Extensions → Manage Extensions → search "Qt Visual Studio Tools"**, then register your Qt path under **Extensions → Qt VS Tools → Qt Versions** (point to `C:\Qt\6.11.1\msvc2022_64\bin\qmake.exe`). |
| C++ standard | **C++17** | Set via `<LanguageStandard>stdcpp17</LanguageStandard>`. |
| QCustomPlot | **2.1.1 (bundled)** | Sources are already in the repo (`qcustomplot.cpp` / `qcustomplot.h`). Nothing to install. **Licensed separately (GPL / commercial)** — see section 5. |

> There is **no CMake / Makefile** and no Qt `Makefile` flow. This project is
> driven by the Qt-integrated Visual C++ project `BS-3-2a_GUI.vcxproj`.

---

## 2. Clone & first-time setup

```bat
git clone <your-repo-url> BS-3-2a_GUI
cd BS-3-2a_GUI
```

Then verify:
1. Qt 6.11.1 msvc2022_64 exists (default: `C:\Qt\6.11.1\msvc2022_64`).
2. Qt VS Tools 3.5.x is installed and the Qt version is registered (the
   `.vcxproj` looks up a kit named `6.11.1_msvc2022_64`).
3. A VS 2022 **x64** build environment is available.

---

## 3. Building

### Option A — Command line

Open a **"Developer Command Prompt for VS 2022"** (x64) and run, from the
project folder:

```bat
msbuild "BS-3-2a_GUI.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
```

Output binary:

```
x64\Release\BS-3-2a_GUI.exe
```

### Option B — Visual Studio IDE

1. Open `BS-3-2a_GUI.slnx`.
2. Set the solution configuration to **Release / x64**.
3. **Build → Build Solution**.

If the build can't find the Qt MSBuild targets, make sure the Qt VS Tools
extension is installed (it supplies `QtMsBuild\qt.props` / `qt.targets`) and
that Qt is registered under **Qt VS Tools → Qt Versions**.

---

## 4. Running

### Quick run (development)

Qt DLLs are on the Qt bin folder — add them to `PATH` before launching:

```powershell
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;" + $env:PATH
.\x64\Release\BS-3-2a_GUI.exe
```

> Run from the project directory — some diagnostic outputs (`nav_diag.txt`)
> are written to relative/absolute repo paths.

### Distributing the exe (optional)

Bundle the Qt runtime DLLs alongside the exe with Qt's deployment tool:

```bat
"C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe" x64\Release\BS-3-2a_GUI.exe
```

### Input data

The app post-processes a recorded BDS-3 B2a **raw-IF** file (`.bin` / `.dat`).
Sample data (`dump1_ch3_1.bin`) is **not committed** to the repo — provide
your own capture and load it in the GUI (Browse...). Typical defaults used
with the sample file:

- Sampling frequency: **99.375 MHz**
- Intermediate frequency (IF): **13.55 MHz**
- BDS-3 B2a code: **10,230 chips @ 10.23 MHz**
- Satellite list: e.g. `19 20 30 37 40 43`

Plot tabs are populated automatically as the pipeline stages complete:

- **Acquisition** tab → plots as soon as acquisition finishes.
- **Tracking** tab → per-PRN via the channel-table **View** button (opens an
  independent popup per PRN so satellites can be compared side by side).
- **C/N0**, **Navigation Position Track** and **Sky View Polar Plot** → after
  tracking/navigation.
- The Navigation and Sky-view tabs only render plots when **≥ 4 satellites**
  are tracked (a position fix needs ≥ 4); otherwise they show a notice.

### Diagnostic mode

A headless diagnostics run is built in. It takes ~6–8 min (6 channels × 30 s)
and appends results to `nav_diag.txt`:

```powershell
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;" + $env:PATH
.\x64\Release\BS-3-2a_GUI.exe --nav-diag
```

Useful markers in `nav_diag.txt`: `EPHEM PRN=xx OK/FAILED`, `SKY PRN=xx az= el=`.

---

## 5. Third-party licensing

- **Qt 6.11.1** — LGPL v3 / commercial. Not redistributed here; it must be
  installed separately (see section 1).
- **QCustomPlot 2.1.1** — shipped in this repository (`qcustomplot.cpp`,
  `qcustomplot.h`). It is **GPL-licensed / commercially licensed**; the
  LGPL Qt licence does **not** cover it. Review QCustomPlot's licence
  (header comment in `qcustomplot.h`) before distributing your build. For a
  commercial app, buy a QCustomPlot licence or replace it with an LGPL-shipped
  chart library.

---

## 6. Project layout (source)

```
Acquisition.cpp/.h        Serial acquisition search (FFT-based)
Tracking.cpp/.h           Code & carrier tracking loops (DLL/PLL), C/N0
PostProcessor.cpp/.h      Pipeline driver: acq → tracking → nav
PostNavigation.cpp/.h     PVT solution + best-effort sky az/el
EphemerisDecoder.cpp/.h   B-CNAV2 bit-sync, preamble sync, CRC-24Q, decode
Satpos.cpp/.h             BDS-3 ephemeris → satellite ECEF positions
CommonBS2.cpp/.h          cart2geo, topocent, least-square PVT, pseudoranges
SettingsWindow.cpp/.h     Main control window (settings + run pipeline)
SDRPipelineWorker.cpp/.h  Pipeline on a worker QThread (non-blocking GUI)
SDRPlotWindow.cpp/.h      Interactive plots (acq bars, tracking panels, sky view)
IQScatterPlotWidget.cpp   Custom I/Q constellation widget (pan/zoom)
qcustomplot.cpp/.h        QCustomPlot 2.1.1 (bundled, 3rd party)
main.cpp                  Entry point (incl. --nav-diag mode)
```