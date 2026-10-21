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
REM Path variables stay out of parenthesized blocks: a ")" in a path such as
REM "C:\Program Files (x86)" would end the block.

set "FB2K_PATH=%~1"
if "%FB2K_PATH%"=="" set "FB2K_PATH=C:\foobar2000"

REM foobar2000 keeps components and settings in its profile folder: "profile" in
REM a portable installation, otherwise %APPDATA%\foobar2000-v2.
set "PROFILE_DIR=%APPDATA%\foobar2000-v2"
if exist "%FB2K_PATH%\portable_mode_enabled" set "PROFILE_DIR=%FB2K_PATH%\profile"

echo.
echo MilkDrop 2 Plus Uninstaller
echo ===========================
echo.
echo This will remove the plugin, config, and optionally presets from:
echo   "%PROFILE_DIR%"
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

REM Remove plugin directories (all names the component has had)
for %%d in (foo_milkdrop2_plus milkdrop2_plus foo_vis_milk2) do call :remove_dir "%PROFILE_DIR%\user-components-x64\%%d" "plugin directory %%d"

REM Remove config files
call :remove_file "%PROFILE_DIR%\browser_config.ini" "browser_config.ini"
call :remove_file "%PROFILE_DIR%\favorites.ini" "favorites.ini"

REM Remove the milkdrop2 folder, or only the preset scan's leftovers in its default preset folder
if not exist "%PROFILE_DIR%\milkdrop2" goto :done
echo.
set /p DEL_PRESETS="Also delete presets and milkdrop2 data folder? (Y/N): "
if /i not "%DEL_PRESETS%"=="Y" goto :keep_presets
call :remove_dir "%PROFILE_DIR%\milkdrop2" "milkdrop2 data directory"
goto :done
:keep_presets
call :remove_file "%PROFILE_DIR%\milkdrop2\presets\preset_cache.dat" "preset_cache.dat"
call :remove_file "%PROFILE_DIR%\milkdrop2\presets\scan_timing.log" "scan_timing.log"
call :remove_file "%PROFILE_DIR%\milkdrop2\scan_timing.log" "scan_timing.log"

:done
echo.
echo Uninstall complete.
echo.
pause
exit /b 0

REM %1: folder to remove, %2: what to call it
:remove_dir
if not exist "%~1" exit /b 0
rmdir /s /q "%~1"
echo   Removed %~2
exit /b 0

REM %1: file to remove, %2: what to call it
:remove_file
if not exist "%~1" exit /b 0
del "%~1"
echo   Removed %~2
exit /b 0
