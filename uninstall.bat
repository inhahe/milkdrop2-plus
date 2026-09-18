@echo off
REM ============================================================================
REM MilkDrop 2 Plus - Uninstall Script
REM ============================================================================
REM
REM Removes the MilkDrop 2 Plus plugin and all its data from foobar2000.
REM
REM Usage: uninstall.bat [foobar2000_path]
REM   Default foobar2000 path: C:\foobar2000
REM ============================================================================

set FB2K_PATH=%~1
if "%FB2K_PATH%"=="" set FB2K_PATH=C:\foobar2000

echo.
echo MilkDrop 2 Plus Uninstaller
echo ===========================
echo.
echo This will remove the plugin, config, and optionally presets from:
echo   %FB2K_PATH%
echo.

REM Check that foobar2000 is not running
tasklist /fi "imagename eq foobar2000.exe" 2>nul | find /i "foobar2000.exe" >nul
if %errorlevel% equ 0 (
    echo ERROR: foobar2000 is running. Please close it first.
    pause
    exit /b 1
)

set /p CONFIRM="Are you sure? (Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo Cancelled.
    pause
    exit /b 0
)

echo.
echo Removing...

REM Remove plugin directories (all possible names)
for %%d in (foo_milkdrop2_plus milkdrop2_plus foo_vis_milk2) do (
    if exist "%FB2K_PATH%\profile\user-components-x64\%%d" (
        rmdir /s /q "%FB2K_PATH%\profile\user-components-x64\%%d"
        echo   Removed plugin directory: %%d
    )
)

REM Remove config files
if exist "%FB2K_PATH%\profile\browser_config.ini" (
    del "%FB2K_PATH%\profile\browser_config.ini"
    echo   Removed browser_config.ini
)

if exist "%FB2K_PATH%\profile\favorites.ini" (
    del "%FB2K_PATH%\profile\favorites.ini"
    echo   Removed favorites.ini
)

REM Remove milkdrop2 config (but ask about presets)
if exist "%FB2K_PATH%\profile\milkdrop2" (
    echo.
    set /p DEL_PRESETS="Also delete presets and milkdrop2 data folder? (Y/N): "
    if /i "!DEL_PRESETS!"=="Y" (
        rmdir /s /q "%FB2K_PATH%\profile\milkdrop2"
        echo   Removed milkdrop2 data directory
    ) else (
        REM Just remove config files, keep presets
        if exist "%FB2K_PATH%\profile\milkdrop2\preset_cache.dat" (
            del "%FB2K_PATH%\profile\milkdrop2\preset_cache.dat"
            echo   Removed preset_cache.dat
        )
        if exist "%FB2K_PATH%\profile\milkdrop2\scan_timing.log" (
            del "%FB2K_PATH%\profile\milkdrop2\scan_timing.log"
            echo   Removed scan_timing.log
        )
    )
)

echo.
echo Uninstall complete.
echo.
pause
