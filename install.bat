@echo off
REM ============================================================================
REM MilkDrop 2 Visualization for foobar2000 - Installation Script
REM ============================================================================
REM
REM This script installs the MilkDrop 2 plugin into foobar2000.
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

echo.
echo MilkDrop 2 Installer for foobar2000
echo ====================================
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

echo.
echo ============================================================================
echo Installation complete!
echo.
echo IMPORTANT: You need .milk preset files to use MilkDrop.
echo.
echo If you don't have presets yet, download the Cream of the Crop collection:
echo   https://github.com/projectM-visualizer/presets-cream-of-the-crop
echo.
echo Extract the category folders (Fractal, Geometric, etc.) directly into:
echo   %MILKDROP_DIR%\presets\
echo.
echo Then start foobar2000, go to View ^> Layout ^> Enable Layout Editing,
echo right-click a panel, and add MilkDrop from the Visualization category.
echo ============================================================================
echo.
pause
