@echo off
REM Developer shortcut: builds Debug x64, copies the component into a portable
REM foobar2000, and starts it. Usage: cp.bat [foobar2000_path], default C:\foobar2000.
set "FB2K=%~1"
if "%FB2K%"=="" set "FB2K=C:\foobar2000"
set "DEST=%FB2K%\profile\user-components-x64\foo_milkdrop2_plus"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do set "MSBUILD=%%i"
"%MSBUILD%" "%~dp0milkdrop2-plus.sln" -p:Configuration=Debug -p:Platform=x64 -p:PostBuildEventUseInBuild=false -m -verbosity:minimal || exit /b 1
if not exist "%DEST%\data" mkdir "%DEST%\data"
copy /y "%~dp0Bin\x64\Debug\foo_milkdrop2_plus.dll" "%DEST%\" || exit /b 1
copy /y "%~dp0external\winamp\data\*.fx" "%DEST%\data\" >nul
copy /y "%~dp0vis_milk2\help.bin" "%DEST%\data\" >nul
start "" "%FB2K%\foobar2000.exe"
