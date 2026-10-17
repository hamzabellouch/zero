# Zero

<h3 align="center"> Fast, Lightweight Cross-Platform Portable Uninstaller, Deep Residual Cleaner & Real-Time Process Manager.</h3>

<p align="center">
Cleanly uninstall desktop and store applications, eradicate leftover files and registry keys, and monitor system processes in real time with zero installation, zero bloat, and complete privacy.
</p>

<img width="2724" height="1536" alt="Zero" src="https://github.com/user-attachments/assets/c741fb2b-e1e1-445c-b417-f5b9c1d63f5e" />



## Overview

**Zero** is a modern, high-performance, and privacy-focused cross-platform utility designed to give users complete control over their installed software and active system processes. It seamlessly combines a deep application uninstaller, an intelligent residual cleaner, and a real-time task manager into a single lightweight, portable tool.

Unlike traditional utilities that bundle intrusive advertisements, background telemetry services, or heavy installer packages, Zero operates **100% on-device** and offline without collecting or transmitting any personal information or telemetry data. On Windows, it runs as a pure standalone native executable compiled with a static C++ runtime (`/MT`), launching in under 100 milliseconds with an ultra-low memory footprint under 10 MB. On Linux and macOS, Zero delivers the same powerful scanning and management capabilities through a unified, native Qt6 interface.

Zero is built following modern C++17 standards, featuring an intelligent **Sibling Application Protection** engine that protects shared vendor components during leftover cleanup, safe **Recycle Bin integration** for recoverable file deletions, and a responsive dark user interface with Per-Monitor High-DPI scaling.



## Screenshots

Zero UI & Features:

<div align="center">
  <div>
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/1.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/2.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/3.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/4.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/5.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/6.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/7.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/8.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/9.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/10.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/11.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/12.jpg" width="30%" />
    <img src="https://github.com/hamzabellouch/zero/blob/main/Images/13.jpg" width="30%" />
  </div>
</div>

<br>



## ⭐ Key Features & Capabilities

| Feature | Method / API Used | Performance & Speed | Privacy & Safety Level | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Desktop Application Uninstallation** | `AppManager::EnumerateDesktopApps` & Win32 Registry APIs | **Instant** (< 200 ms) | **100% Local / Safe** | Discovers installed 32-bit and 64-bit desktop programs, calculates accurate folder disk sizes, extracts crisp high-DPI icons, and initiates standard or silent uninstallers. |
| **Windows Store (UWP/MSIX) Apps** | `AppManager::EnumerateStoreApps` & PowerShell AppX Cmdlets | **Fast** (< 1 sec) | **100% Local** | Lists modern Windows Store packages with package family metadata and uninstalls them cleanly via `Remove-AppxPackage` without leaving store artifacts. |
| **Deep Residual & Leftover Scanner** | `Cleaner::ScanLeftovers`, File & Registry Traversal | **Rapid** (< 3 sec) | **100% On-Device** | Performs comprehensive recursive scans of `Program Files`, `AppData`, `ProgramData`, and `HKCU`/`HKLM\Software` trees to detect leftover files and registry keys. |
| **Sibling Application Protection** | `Cleaner::GenerateSiblingExclusions` & Token Analyzer | **Instant** (< 5 ms) | **High Safety** | Heuristically detects related sibling applications from the same publisher (e.g. Adobe Acrobat vs. Photoshop) to prevent accidental deletion of shared vendor resources. |
| **Safe Residual Deletion & Recovery** | `Cleaner::DeleteSelectedItems` & `SHFileOperationW` (`FOF_ALLOWUNDO`) | **Instant** | **Reversible / Safe** | Safely routes removed leftover files and folders directly to the OS Recycle Bin for undoability, while cleanly purging orphaned registry keys and values. |
| **Real-Time Task & Process Engine** | `ProcessManager::UpdateSnapshot`, `PSAPI` & `Toolhelp32` | **Real-Time** (1 Hz refresh) | **100% Local** | Monitors active processes with per-process and system-wide CPU utilization %, RAM working set, disk I/O throughput (B/s), and network traffic rate. |
| **Process Instance Grouping & Tree Kill** | `ProcessManager::TerminateProcessGroup` & Win32 Process APIs | **Instant** (< 50 ms) | **Controlled** | Groups multi-process software (browsers, Electron runtimes) into single aggregated rows and provides one-click termination of full process trees. |
| **High-Performance Search & Filter** | Instant Substring Token Filter & Owner-Drawn Lists | **Zero Latency** (Instant) | **100% On-Device** | Filters hundreds of installed programs and processes instantly on keystroke with custom owner-drawn capsule controls and zero UI stutter. |
| **Portable Zero-Footprint Standalone Binary** | Statically linked MSVC (`/MT`), Single `.exe` (< 5 MB) | **Blazing Fast** (< 100 ms launch) | **Zero System Footprint** | Requires no installation wizard, no external DLL dependencies, no background services, and writes zero configuration clutter to the host system. |
| **Modern Dark Theme & High-DPI UI** | Double-buffered GDI+ / Win32 Custom Draw & Qt6 Palette | **Smooth 60+ FPS** | **N/A** | Features an elegant dark visual aesthetic (`#191919` / `#212121` with `#E0562E` vivid coral accents) and dynamic Per-Monitor High-DPI (v2) scaling. |
| **Cross-Platform Architecture (Linux & macOS)** | Modern Qt6 GUI with native platform engines (`/proc`, `libproc`) | **Native Speed** | **100% Local** | Seamlessly extends application discovery, deep cleaning, and real-time process monitoring across Linux distributions and macOS environments. |
| **Zero Telemetry / 100% Privacy** | Pure Local Code, No Network Sockets, No Trackers | **N/A** | **Complete Privacy** | Zero analytics SDKs, zero crash report collectors, zero advertising frameworks, and zero outbound network transmission. |



## 🛠 Tech Stack & Architecture

Zero follows clean architecture principles for maximum execution speed, safety, minimal memory usage, and zero system dependencies:

* **Core Architecture & Concurrency:**
  - Decoupled Clean Architecture separating OS application scanning (`IAppScanner`), process telemetry engines (`IProcessEngine`), and residual cleaners (`ILeftoverCleaner`)
  - Asynchronous background worker threads (`std::thread`, `std::atomic<bool>`) with interactive cancel tokens and live stage progress reporting (`ScanProgressCallback`)
  - High-throughput differential sampling for real-time CPU percentages, memory working sets, and disk/network I/O transfer rates

* **Language & Build Tooling:**
  - `100% Modern C++17` (`/std:c++17`) with strict type safety, RAII memory management, and zero garbage collection overhead
  - Windows: `Microsoft Visual C++ (MSVC 2022)` with `/MT` (Multi-threaded Static CRT) and `/O2` maximum speed optimizations for a standalone zero-dependency executable
  - Linux & macOS: `CMake 3.16+` cross-platform build orchestration with automated `CPack` distribution packaging (TGZ, DEB, RPM, DMG, DragNDrop)

* **UI Framework & Design System:**
  - Windows Native: Win32 API, GDI+, Custom Double-Buffered Owner-Drawn ListView (`LVS_OWNERDRAWFIXED`), custom dark menu bar, search capsule, and anti-aliased typography
  - Dynamic Per-Monitor High-DPI (V2) awareness scaling smoothly across 100%, 125%, 150%, 175%, and 200% displays
  - Linux & macOS: Modern `Qt6` / `Qt5` (Widgets, Core, Gui) interface with unified dark styling and native window integration

* **Platform APIs & System Engines:**
  - Windows: `Win32 Registry API` (`RegOpenKeyExW`, `RegEnumKeyExW`, `RegDeleteKeyW`), `PSAPI` (`GetProcessMemoryInfo`), `Toolhelp32` (`CreateToolhelp32Snapshot`), `Iphlpapi` & `GetProcessIoCounters`, `ShellAPI` (`SHFileOperationW`, `PrivateExtractIconsW`), `DWM` & `UxTheme`
  - Linux: `/proc` pseudo-filesystem parser, XDG desktop entry specification reader, and native package manager integration
  - macOS: `libproc` process inspection API, Cocoa and IOKit hardware/system frameworks, and macOS Application bundle inspector

* **Safety, Recovery & Data Integrity:**
  - Protected system directory blocklists (`Windows`, `System32`, `Microsoft`, etc.) preventing unauthorized system modifications
  - Safe Recycle Bin integration (`SHFileOperationW` with `FOF_ALLOWUNDO`) ensuring non-destructive leftover removal
  - Heuristic token isolation preventing accidental collisions with sibling applications



## 🔥 Installation

1. Go to the Releases page:
   https://github.com/hamzabellouch/zero/releases

2. Download the latest binary for your operating system (`Zero.exe` for Windows, `.tar.gz` for Linux, or `.dmg` for macOS).

3. Zero is **100% portable**: run the executable directly from any folder or USB flash drive — no installation wizard required.

4. *(Recommended on Windows)*: Run as Administrator to allow full access for launching uninstallers and cleaning protected system registry keys.

## 🔨 Building from Source

### Windows (Native Standalone Executable)
To build the standalone Windows binary, make sure you have Visual Studio 2022 with C++ Desktop Development tools installed:

1. Clone the repository: `git clone https://github.com/hamzabellouch/zero.git`
2. Open a standard terminal or Developer Command Prompt for Visual Studio.
3. Run the automated build script:
   ```cmd
   .\build.bat
   ```
4. The optimized standalone binary will be generated at `bin\Zero.exe`.

### Linux & macOS (Cross-Platform Qt6 Target)
To build the cross-platform Qt6 application on Linux or macOS:

1. Clone the repository: `git clone https://github.com/hamzabellouch/zero.git`
2. Configure and build via CMake:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
3. *(Optional)* Package for distribution:
   ```bash
   cd build && cpack
   ```



> [!WARNING]
> There is always a possibility of error, so we assume no responsibility for any inaccuracies.


### <a name="Copyright©2026"></a> Copyright © 2026

Thank you for checking out Zero. If you have any feedback or suggestions, feel free to contact us:
hamzabellouchcontact@gmail.com

Stay connected and follow us on:  
[WhatsApp](https://whatsapp.com/channel/0029Vb7MArw0LKZMpjjqOk2P) | [Facebook](https://facebook.com/hamzabellouch0) | [Instagram](https://instagram.com/hamzabellouch0) | [Twitter](https://twitter.com/hamzabellouch0) | [Telegram](https://t.me/hammzabellouch) | [LinkedIn](https://www.linkedin.com/in/hamzabellouch)
