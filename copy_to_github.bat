@echo off
REM ============================================================================
REM Copy project files to D:\github\foobar2000milkdrop for publishing.
REM Excludes build artifacts, NuGet packages, and external SDK files that
REM must be downloaded separately.
REM ============================================================================

set SRC=%~dp0
set DST=D:\github\milkdrop2-plus

echo.
echo Copying project to %DST%...
echo.

if not exist "%DST%" mkdir "%DST%"

REM Copy root files
for %%f in (
    .clang-format
    .doxyfile
    .gitattributes
    .gitignore
    .vsconfig
    BUILDING.md
    CHANGELOG.md
    foo_vis_milk2.sln
    LICENSE.txt
    LICENSE-ADDITIONS.txt
    LICENSES.md
    nuget.config
    README.md
    TESTING.md
    install.bat
    copy_to_github.bat
) do (
    if exist "%SRC%%%f" copy /y "%SRC%%%f" "%DST%\" >nul 2>nul
)

REM Copy foo_vis_milk2 source
echo Copying foo_vis_milk2\...
if not exist "%DST%\foo_vis_milk2" mkdir "%DST%\foo_vis_milk2"
xcopy /y /s /q "%SRC%foo_vis_milk2\*.cpp" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.h" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.rc" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.hlsl" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.fxh" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.ico" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.manifest" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.vcxproj" "%DST%\foo_vis_milk2\" >nul
xcopy /y /s /q "%SRC%foo_vis_milk2\*.vcxproj.filters" "%DST%\foo_vis_milk2\" >nul
if exist "%SRC%foo_vis_milk2\packages.config" copy /y "%SRC%foo_vis_milk2\packages.config" "%DST%\foo_vis_milk2\" >nul

REM Copy vis_milk2 source
echo Copying vis_milk2\...
if not exist "%DST%\vis_milk2" mkdir "%DST%\vis_milk2"
xcopy /y /s /q "%SRC%vis_milk2\*.cpp" "%DST%\vis_milk2\" >nul
xcopy /y /s /q "%SRC%vis_milk2\*.h" "%DST%\vis_milk2\" >nul
xcopy /y /s /q "%SRC%vis_milk2\*.hlsl" "%DST%\vis_milk2\" >nul
xcopy /y /s /q "%SRC%vis_milk2\*.bin" "%DST%\vis_milk2\" >nul
xcopy /y /s /q "%SRC%vis_milk2\*.vcxproj" "%DST%\vis_milk2\" >nul
xcopy /y /s /q "%SRC%vis_milk2\*.vcxproj.filters" "%DST%\vis_milk2\" >nul
if exist "%SRC%vis_milk2\packages.config" copy /y "%SRC%vis_milk2\packages.config" "%DST%\vis_milk2\" >nul

REM Copy test project
echo Copying test\...
if not exist "%DST%\test" mkdir "%DST%\test"
xcopy /y /s /q "%SRC%test\*.*" "%DST%\test\" >nul

REM Copy external files (patches, winamp headers/shaders, nu utilities)
echo Copying external\...
if not exist "%DST%\external" mkdir "%DST%\external"
for %%f in (fb2ksdk.patch pmeel.patch dxtk2019.patch dxtkwin10.patch) do (
    if exist "%SRC%external\%%f" copy /y "%SRC%external\%%f" "%DST%\external\" >nul
)

REM Copy winamp headers and data
if not exist "%DST%\external\winamp" mkdir "%DST%\external\winamp"
xcopy /y /s /q "%SRC%external\winamp\*.*" "%DST%\external\winamp\" >nul

REM Copy nu utilities
if not exist "%DST%\external\nu" mkdir "%DST%\external\nu"
xcopy /y /s /q "%SRC%external\nu\*.*" "%DST%\external\nu\" >nul

REM Copy eel2 project
if exist "%SRC%external\eel2" (
    if not exist "%DST%\external\eel2" mkdir "%DST%\external\eel2"
    xcopy /y /s /q "%SRC%external\eel2\*.*" "%DST%\external\eel2\" >nul
)

REM Copy .github workflows
if exist "%SRC%.github" (
    echo Copying .github\...
    if not exist "%DST%\.github" mkdir "%DST%\.github"
    xcopy /y /s /q "%SRC%.github\*.*" "%DST%\.github\" >nul
)

REM Copy tools
if exist "%SRC%tools" (
    echo Copying tools\...
    if not exist "%DST%\tools" mkdir "%DST%\tools"
    xcopy /y /s /q "%SRC%tools\*.*" "%DST%\tools\" >nul
)

REM Copy release build
echo Copying release build...
if not exist "%DST%\Bin\x64\Release" mkdir "%DST%\Bin\x64\Release"
if exist "%SRC%Bin\x64\Release\foo_vis_milk2.dll" (
    copy /y "%SRC%Bin\x64\Release\foo_vis_milk2.dll" "%DST%\Bin\x64\Release\" >nul
)

echo.
echo ============================================================================
echo Done! Project copied to %DST%
echo.
echo NOTE: The following must be downloaded separately (not included):
echo   - foobar2000 SDK (external\foobar2000\, external\pfc\, external\libppui\)
echo   - projectm-eval  (external\projectm-eval\)
echo   - NuGet packages  (DirectXTK, WTL)
echo See BUILDING.md for setup instructions.
echo ============================================================================
echo.
pause
