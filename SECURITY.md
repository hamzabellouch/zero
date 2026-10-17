# Security Policy

## Supported Versions

The following versions of Zero are currently supported with security, stability, and privacy updates.

| Version       | Supported          |
| ------------- | ------------------ |
| + 0.0.1       | :white_check_mark: |
| < 0.0.1       | :x:                |



## Reporting a Vulnerability

If you discover a security vulnerability, unexpected file deletion behavior, privacy issue, or any bug that could negatively affect user system stability or data integrity, please report it responsibly.

### Before Reporting
Please make sure that:
- The issue is reproducible
- You are using the latest supported version of Zero
- The issue is not caused by third-party modifications, corrupted operating system installations, or unofficial repackaged builds

### How to Report
You can report vulnerabilities through:
- GitHub Issues (for non-sensitive reports and general bug reports)
- E-mail: hamzabellouchcontact@gmail.com / [Froms](https://docs.google.com/forms/d/e/1FAIpQLSf87zkBsPRiUX19qF42vekAwgV_bW2EWZZPEThTo8PFIOFc0w/viewform?usp=header) 

When reporting, please include:
- Operating system and version/architecture (e.g., Windows 11 23H2 x64, Ubuntu 24.04 LTS, macOS 14 Sonoma)
- Zero application version and build target (Native Win32 Standalone or Qt6 Cross-Platform)
- Execution privilege level (Standard User vs. Run as Administrator / elevated)
- Steps to reproduce the issue
- Screenshots, crash logs, or console error messages if available
- A clear explanation of the potential security, system stability, or data safety impact

### Response Policy
Security reports are reviewed as quickly as possible.  
If the issue is confirmed:
- The vulnerability will be investigated, verified, and patched
- A fix will be included in the next update release
- Credit may be given to the reporter if requested

If the report is invalid, incomplete, or not reproducible, it may be closed without action.



## Security & Architecture Notes

Zero is designed with a strict **Privacy-First & On-Device Security Model**:

- **Local Execution:** Software uninstallation, residual cleaning, and process monitoring operate 100% locally on your computer. No software inventories, process metrics, or telemetry data is ever transmitted to external servers.
- **Safe Residual Deletion:** Leftover files and folders identified during cleanup are sent directly to the operating system's Recycle Bin (`SHFileOperationW` with `FOF_ALLOWUNDO`), ensuring users can safely recover any unintentionally removed files.
- **Sibling Application Protection:** An intelligent tokenized heuristic engine isolates sibling software from the same publisher (e.g. Adobe Acrobat vs. Adobe Photoshop), guaranteeing that shared vendor libraries and sibling installations are never touched.
- **System Directory Isolation:** Zero actively prevents scanning or deleting core operating system paths (`Windows`, `System32`, `Microsoft`, etc.) and protected registry hives via hardcoded safety safeguards.
- **Controlled Privilege Model:** Zero operates safely under standard user permissions for user-level software and requests administrative elevation only when removing system-wide software or protected registry keys.
- **Transient Memory Handling:** All process snapshots, resource counters, and leftover scan lists are maintained solely in volatile memory (RAM) and are immediately discarded upon dialog closure or exit, keeping system footprint minimal and secure.
