@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Building Zero (Portable Standalone)
echo ===================================================

:: Check if cl.exe is already in PATH (e.g. GitHub Actions or Developer Command Prompt)
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [1/3] MSVC environment already configured in PATH.
    goto :compile_prep
)

:: Otherwise, locate vcvars64.bat dynamically using vswhere or standard locations
set "VS_VCVARS="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if "!VS_VCVARS!"=="" (
    for %%e in (Community Professional Enterprise BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if "!VS_VCVARS!"=="" (
    echo [ERROR] Could not find vcvars64.bat. Please run from Developer Command Prompt for VS.
    exit /b 1
)

echo [1/3] Initializing Visual Studio Build Environment (x64)...
call "!VS_VCVARS!" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment.
    exit /b 1
)

:compile_prep

if not exist "bin" mkdir "bin"
if not exist "build" mkdir "build"

echo [2/3] Compiling Application Resources and Manifest...
cd src
rc.exe /nologo /fo "..\build\app.res" app.rc
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    cd ..
    exit /b 1
)
cd ..

echo [3/3] Compiling and Linking C++ Sources (Optimized /MT Standalone)...
cl.exe /nologo /O2 /W3 /MT /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    /Fo"build\\" ^
    /Fe"bin\Zero.exe" ^
    src\main.cpp src\app_manager.cpp src\cleaner.cpp src\ui_leftovers_dlg.cpp src\ui_scan_dlg.cpp src\process_manager.cpp ^
    build\app.res ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
    /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /OPT:REF /OPT:ICF ^
    user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib advapi32.lib ole32.lib dwmapi.lib uxtheme.lib psapi.lib version.lib iphlpapi.lib

if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

echo.
echo ===================================================
echo   BUILD SUCCESSFUL!
echo   Executable generated at: bin\Zero.exe
echo ===================================================
dir "bin\Zero.exe"

exit /b 0
