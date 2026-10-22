# Building the MilkDrop 2 Visualization Component for foobar2000

## Using the GUI

A step-by-step on how to compile on Windows:

1. Install [Visual Studio 2026](https://visualstudio.microsoft.com/vs/) (or its Build Tools) and [Git](https://git-scm.com/). The projects build with its v145 toolset.
   1. Clone the repository.
   ```powershell
   git.exe clone https://github.com/jecassis/foo_vis_milk2.git -b master --depth 1 foo_vis_milk2
   ```
   2. In Visual Studio "_Open a project or solution_" and point it to the [`foo_vis_milk2.sln`](foo_vis_milk2.sln) file in the repository.
   3. Ensure all additional tools for Visual Studio are installed by running the installer using **Tools > Get Tools and Features...**. In the installer that comes up hit the "_More_" dropdown and select "_Import configuration_". In the file selection dialog, point it to [`.vsconfig`](.vsconfig) in the repository to install things like NuGet, ATL and a few other prerequisites.
      > Note: Additional debug tools such as those for graphics and memory debugging are not included in this `.vsconfig`, however they might be useful for development.
   4. In Solution Explorer, if the "*foo_vis_milk2*" project is not bold font, right click on it and select "_Set as Starter Project_".
   5. In Solution Explorer again, right click on the "*Solution 'foo_vis_milk2'*" tree root at the top and select "_Manage NuGet Packages for Solution_". Use the tab that comes up to install WTL for the various projects (DirectXTK's few sources the component uses are in `external\dxtk` and build with the rest); they should be pre-populated but you can search if not.
2. Download the foobar2000 SDK and place it in the `external\` directory according to the file layout in the [README](README.md#repository-notes). Then use the CLI to patch it:

```powershell
Set-Location -Path "path\to\repo"
Set-Location -Path "$(Get-Location)\external"
git.exe init
git.exe add foobar2000\ libppui\ pfc\
git.exe commit -m 'Initial commit'
git.exe apply --ignore-whitespace --whitespace=nowarn --verbose "$(Get-Location)\fb2ksdk.patch"
Remove-Item -Path "$(Get-Location)\.git" -Recurse -Force
```

3. Clone the projectm-eval repo into the `external\` directory and patch it in a similar manner to the SDK:

```powershell
Set-Location -Path "path\to\repo"
Set-Location -Path "$(Get-Location)\external"
git.exe clone https://github.com/projectM-visualizer/projectm-eval.git
Set-Location -Path "$(Get-Location)\external\projectm-eval"
git.exe apply --ignore-whitespace --whitespace=nowarn --verbose "$(Get-Location)\..\pmeel.patch"
```

4. Then just select your desired build configuration and platform in Visual Studio, for example "_Debug x64_" and do **Build > Build Solution** (**Ctrl+Shift+B**).
5. For quick debugging here is a suggestion for `foo_vis_milk2/foo_vis_milk2.vcxproj.user` (x64 only; set `Fb2kDir` to your installation). It runs foobar2000 under the debugger, and copies each build, with the shader files the engine reads at start, into the installation's components folder. It copies only if `Fb2kDir` holds `foobar2000.exe`, so that a wrong path does not make a folder that is not an installation. Point it at a foobar2000 kept for testing, not the one you listen with: a Debug build makes foobar2000 ask at every start whether to load it, and the copy fails while foobar2000 runs.

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="Current" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <Fb2kDir>$(LocalAppData)\Programs\foobar2000_x64</Fb2kDir>
    <Fb2kComponentDir>$(Fb2kDir)\profile\user-components-x64\foo_milkdrop2_plus</Fb2kComponentDir>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Platform)'=='x64'">
    <LocalDebuggerCommand>$(Fb2kDir)\foobar2000.exe</LocalDebuggerCommand>
    <LocalDebuggerDebuggerType>NativeOnly</LocalDebuggerDebuggerType>
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Platform)'=='x64'">
    <PostBuildEvent>
      <Command>if exist "$(Fb2kDir)\foobar2000.exe" if not exist "$(Fb2kComponentDir)\data" mkdir "$(Fb2kComponentDir)\data"
if exist "$(Fb2kDir)\foobar2000.exe" copy /y "$(TargetPath)" "$(Fb2kComponentDir)\"
if exist "$(Fb2kDir)\foobar2000.exe" copy /y "$(SolutionDir)external\winamp\data\*.fx" "$(Fb2kComponentDir)\data\"
if not exist "$(Fb2kDir)\foobar2000.exe" echo No foobar2000.exe in $(Fb2kDir): the component was not copied.</Command>
      <Message>Copying "$(TargetFileName)" to the foobar2000 installation in $(Fb2kDir), if there is one...</Message>
    </PostBuildEvent>
  </ItemDefinitionGroup>
</Project>
```

6. To launch foobar2000 while attaching the Visual Studio debugger do **Debug > Start Debugging** (**F5**).

## Using the CLI

See [build.yml](.github/workflows/build.yml).