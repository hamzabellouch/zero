# Contributing

Thank you for your interest in contributing to **Zero**! Zero is a fast, lightweight, and cross-platform portable uninstaller, deep residual cleaner, and real-time process manager built with modern C++17, native Win32 APIs, and cross-platform Qt6.

Before submitting a bug report or feature request, please search existing issues (including closed ones) to ensure it hasn't already been reported or discussed. If there are no duplicates, feel free to submit a new issue using the appropriate template.

**Please note:** Issues that do not use existing templates or lack sufficient detail may be closed without review.

For questions or any other ideas to improve, you can join our official e-mail : hamzabellouchcontact@gmail.com or [Social Media platforms](https://sites.google.com/view/hamzabellouch/).


## Disclaimer

Zero is an active open-source project focused on providing a clean, safe, lightning-fast, and privacy-first application uninstaller and process management tool. While we strive for uncompromising system safety, code quality, and reliability, contributions and feedback are always welcome to improve scanning depth, compatibility, and user experience.



## Bug Reports

When submitting a bug report, please make sure your issue contains **sufficient information** to reproduce the problem. Useful details include:

- Operating system and version/architecture (e.g., Windows 11 23H2 x64, Ubuntu 24.04 LTS, macOS 14 Sonoma)
- Zero application version and build target (Native Win32 Standalone or Qt6 Cross-Platform)
- Privilege level when running Zero (Standard User vs. Run as Administrator / elevated)
- Target application or process details (Name, publisher, whether standard desktop or Store app)
- Steps to reproduce the bug
- Terminal/console output, error messages, or screenshots if applicable



## Feature Requests

Zero aims to remain a blazing-fast, privacy-focused, zero-bloat, and lightweight portable utility. We welcome suggestions that enhance leftover scanning accuracy, process inspection telemetry, UI responsiveness, or cross-platform capabilities.

When suggesting a new feature, please consider:
- **Relevance:** Does the feature fit the core goal of software uninstallation, residual cleaning, or real-time process monitoring?
- **Safety & Integrity:** Does it preserve operating system stability, prevent accidental deletion of critical system files, and protect user data without introducing unsafe external dependencies?
- **Simplicity & Zero Bloat:** We strictly prioritize instant startup (< 100 ms), zero background overhead, and self-contained portable execution over heavy features that bloat the binary.



## Pull Requests

If you wish to contribute directly by submitting code:

1. **Discuss First:** Leave a comment under an existing issue or open a new issue describing the changes you plan to make before writing code.
2. **Avoid Conflicts:** Comment on the issue to let others know you are working on it to prevent duplicate efforts.
3. **Follow Code Style:** Ensure your code adheres to modern C++17 standards, RAII for memory and handle management, clean separation of platform-specific code within `src/platform/`, and safe API patterns.



## New Contributors

If you are new to the project:
- Browse our open issues for beginner-friendly tasks marked as `good first issue` or `help wanted`.
- Feel free to ask clarifying questions directly on the issue thread!



## Building From Source

To build Zero locally:

1. **Prerequisites:**
   - **Windows Native Build:** Visual Studio 2022 (Community or higher) with Desktop C++ development workload (MSVC x64) and Windows 10/11 SDK.
   - **Cross-Platform Build (Linux / macOS / Windows):** CMake 3.16+, C++17 compatible compiler (GCC, Clang, or MSVC), and Qt6 (or Qt5) development packages (`qt6-base-dev` on Linux, `brew install qt@6` on macOS).

2. **Steps:**
   - **Windows Standalone (`Zero.exe`):**
     ```cmd
     # Clone the repository
     git clone https://github.com/hamzabellouch/zero.git

     # Build standalone binary using the automated script
     .\build.bat
     ```

   - **Cross-Platform (Qt6):**
     ```bash
     # Clone the repository
     git clone https://github.com/hamzabellouch/zero.git

     # Configure and build using CMake
     cmake -B build -DCMAKE_BUILD_TYPE=Release
     cmake --build build --config Release
     ```
