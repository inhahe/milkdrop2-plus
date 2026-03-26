@echo off
REM ============================================================================
REM MilkDrop 2 Plus - Installation Script
REM ============================================================================
REM
REM This script installs the MilkDrop 2 Plus plugin into foobar2000.
REM Run this after building the project (Release x64).
REM
REM Usage: install.bat [foobar2000_path]
REM   Default foobar2000 path: C:\foobar2000
REM ============================================================================

set FB2K_PATH=%~1
if "%FB2K_PATH%"=="" set FB2K_PATH=C:\foobar2000

set COMPONENT_DIR=%FB2K_PATH%\profile\user-components-x64\foo_vis_milk2
set MILKDROP_DIR=%FB2K_PATH%\profile\milkdrop2
set BUILD_DIR=%~dp0Bin\x64\Release
set DATA_DIR=%~dp0external\winamp\data
set PRESETS_DIR=%~dp0presets

echo.
echo MilkDrop 2 Plus Installer for foobar2000
echo ==========================================
echo.
echo foobar2000 path: %FB2K_PATH%
echo Component dir:   %COMPONENT_DIR%
echo MilkDrop dir:    %MILKDROP_DIR%
echo Build dir:       %BUILD_DIR%
echo.

REM Check that foobar2000 exists
if not exist "%FB2K_PATH%\foobar2000.exe" (
    echo ERROR: foobar2000 not found at %FB2K_PATH%
    echo        Pass the foobar2000 install path as the first argument.
    echo        Example: install.bat "C:\Program Files\foobar2000"
    pause
    exit /b 1
)

REM Check that the DLL was built
if not exist "%BUILD_DIR%\foo_vis_milk2.dll" (
    echo ERROR: foo_vis_milk2.dll not found in %BUILD_DIR%
    echo        Build the project first (Release x64).
    pause
    exit /b 1
)

REM Check that foobar2000 is not running
tasklist /fi "imagename eq foobar2000.exe" 2>nul | find /i "foobar2000.exe" >nul
if %errorlevel% equ 0 (
    echo ERROR: foobar2000 is running. Please close it first.
    pause
    exit /b 1
)

echo Installing...
echo.

REM Create directories
if not exist "%COMPONENT_DIR%" mkdir "%COMPONENT_DIR%"
if not exist "%COMPONENT_DIR%\data" mkdir "%COMPONENT_DIR%\data"
if not exist "%MILKDROP_DIR%" mkdir "%MILKDROP_DIR%"
if not exist "%MILKDROP_DIR%\presets" mkdir "%MILKDROP_DIR%\presets"

REM Copy plugin DLL
echo Copying foo_vis_milk2.dll...
copy /y "%BUILD_DIR%\foo_vis_milk2.dll" "%COMPONENT_DIR%\" >nul

REM Copy shader data files
echo Copying shader files...
copy /y "%DATA_DIR%\*.fx" "%COMPONENT_DIR%\data\" >nul

REM Copy help file if it exists
if exist "%~dp0vis_milk2\help.bin" (
    echo Copying help file...
    copy /y "%~dp0vis_milk2\help.bin" "%COMPONENT_DIR%\data\" >nul
)

REM ============================================================================
REM Presets: check submodule, offer to download if missing
REM ============================================================================

REM Check if presets are already installed
set PRESET_COUNT=0
for /f %%a in ('dir /s /b "%MILKDROP_DIR%\presets\*.milk" 2^>nul ^| find /c ".milk"') do set PRESET_COUNT=%%a

if %PRESET_COUNT% gtr 0 (
    echo.
    echo Presets already installed: %PRESET_COUNT% .milk files found.
    echo Skipping preset installation.
    goto :done
)

REM Check if the submodule presets directory has files
if exist "%PRESETS_DIR%\Fractal" (
    echo.
    echo Found presets submodule. Copying presets to foobar2000...
    echo This may take a moment (thousands of files)...
    xcopy /y /s /q "%PRESETS_DIR%\*.*" "%MILKDROP_DIR%\presets\" >nul
    echo Presets copied successfully.
    goto :done
)

REM Presets not found anywhere — offer to download
echo.
echo No presets found. MilkDrop needs .milk preset files to display visualizations.
echo.
echo The Cream of the Crop collection (~10,000 presets) can be downloaded from:
echo   https://github.com/projectM-visualizer/presets-cream-of-the-crop
echo.

REM Check if git is available
where git >nul 2>nul
if %errorlevel% neq 0 (
    echo Git is not installed. Please download the presets manually and extract
    echo the category folders into: %MILKDROP_DIR%\presets\
    goto :done
)

set /p DOWNLOAD="Download presets now using git? (Y/N): "
if /i "%DOWNLOAD%"=="Y" (
    echo.
    echo Downloading presets (this may take a few minutes)...
    git clone --depth 1 https://github.com/projectM-visualizer/presets-cream-of-the-crop.git "%TEMP%\milkdrop-presets-temp" 2>nul
    if exist "%TEMP%\milkdrop-presets-temp" (
        echo Copying presets...
        xcopy /y /s /q "%TEMP%\milkdrop-presets-temp\*.*" "%MILKDROP_DIR%\presets\" >nul
        REM Clean up — remove .git and other non-preset files
        if exist "%MILKDROP_DIR%\presets\.git" rmdir /s /q "%MILKDROP_DIR%\presets\.git" >nul 2>nul
        if exist "%MILKDROP_DIR%\presets\LICENSE" del "%MILKDROP_DIR%\presets\LICENSE" >nul 2>nul
        if exist "%MILKDROP_DIR%\presets\README.md" del "%MILKDROP_DIR%\presets\README.md" >nul 2>nul
        rmdir /s /q "%TEMP%\milkdrop-presets-temp" >nul 2>nul
        echo Presets installed successfully!
    ) else (
        echo Failed to download presets. Please download them manually from:
        echo   https://github.com/projectM-visualizer/presets-cream-of-the-crop
        echo and extract into: %MILKDROP_DIR%\presets\
    )
) else (
    echo.
    echo Skipping preset download. You can download them later from:
    echo   https://github.com/projectM-visualizer/presets-cream-of-the-crop
    echo and extract the category folders into: %MILKDROP_DIR%\presets\
)

:done
echo.
echo ============================================================================
echo Installation complete!
echo.
echo Start foobar2000, go to View ^> Layout ^> Enable Layout Editing,
echo right-click a panel, and add MilkDrop from the Visualization category.
echo ============================================================================
echo.
pause
