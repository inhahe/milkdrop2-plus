# MilkDrop 2 Visualization Component for foobar2000

Port of Winamp's MilkDrop 2 visualization library from its original DirectX 9 version to use DirectX 11.1.
MilkDrop 2 takes you flying through visualizations of the soundwaves you're hearing, and uses beat detection to trigger myriad psychedelic effects, creating a rich visual journey through sound.

Prerequisites to build the `milkdrop2_plus.dll` component for foobar2000:

- [foobar2000 SDK](https://www.foobar2000.org/SDK): download the latest version and uncompress the contents in the `external/` folder and apply the [patch](external/fb2ksdk.patch).
- [NS-EEL2](https://github.com/justinfrankel/WDL/tree/main/WDL/eel2) (included in [WDL](https://www.cockos.com/wdl/)): the files required to build the DLL are included in this repository.
- [projectM EEL](https://github.com/projectM-visualizer/projectm-eval): clone the repository into the `external/` folder, checkout the `HEAD` of the `master` branch and apply the [patch](external/pmeel.patch). _This is the default expression evaluation library._
- [DirectXTK](https://github.com/Microsoft/DirectXTK): the three source files the component uses (common states and the DDS and WIC texture loaders) are included in [`external/dxtk`](external/dxtk), from release `sep2024`, and built with the rest so that they share its statically linked runtime.
- [Windows Template Library (WTL)](https://wtl.sourceforge.io/): the files required to build the DLL are fetched via the NuGet package manager.
- [Visual Studio 2026](https://visualstudio.microsoft.com/vs/) (or its Build Tools): open the [`milkdrop2-plus`](milkdrop2-plus.sln) solution, set `foo_vis_milk2` as the Startup Project, install WTL as a NuGet package, select a configuration, and build the solution.

> Import the Visual Studio [installation configuration](.vsconfig) file to install required components such as the [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/), [Active Template Library (ATL)](https://learn.microsoft.com/en-us/cpp/atl/atl-com-desktop-components) and [NuGet Package Manager](https://www.nuget.org/).

Every project in the tree, including the foobar2000 SDK projects, builds with the v145 toolset and the statically linked C/C++ runtime: [`Directory.Build.props`](Directory.Build.props) and [`Directory.Build.targets`](Directory.Build.targets) set both, whatever the individual project files say. foobar2000 loads one copy of the DLL runtime for the whole process, from its own folder, and that copy can be older than the compiler; a statically linked runtime does not depend on it.

The upstream project's [build pipeline](.github/workflows/build.yml.disabled) shows the build step by step; this fork keeps it only for reference, with GitHub Actions turned off (it predates the fork's changes and failed on every push). _Only x86 and x64 Intel architecture platforms are tested._

See [CHANGELOG](CHANGELOG.md) for additional details.

See [BUILDING](BUILDING.md) for solution build instructions.

See [TESTING](TESTING.md) for an outline of how to run a simple unit test and collect runtime coverage.

See [LICENSES](LICENSES.md) to become thoroughly confused.

Fork additions are licensed under the [MIT License](LICENSE-ADDITIONS.txt) by Inhahe. Original code retains its MPL-2.0 / BSD-3-Clause licenses.

## Features

- Uses DirectX 11.1 (Direct3D 11.1, DXGI 1.6, Direct2D 1.1, DirectWrite 1.1) for rendering.
- Supports the Default User Interface (Default UI) only.
- Configurable through foobar2000 preferences instead of `.ini` files.
- Can build 32-bit and 64-bit x86 component configurations as well as ARM64 and ARM64EC.
- Built for foobar2000 2.0 and later with latest Windows 11 SDK (10.0.26100.0) and MSVC (v145), with the C/C++ runtime linked in, so it does not depend on the runtime version foobar2000 ships.
- Updated all library dependencies to their latest available releases.
- Deprecated or insecure functions have been rewritten and most unused functionality removed.
- `vis_milk2` has been upgraded to use more modern C++ alongside the move to DirectX 11.
- Tested on foobar2000 v2.1.6 (x86 32-bit and x86 64-bit) and Microsoft Windows 11 (Build 26100.1742).
- Requires Windows 10 or later (the minimum for code built with MSVC v145).
  - However, some features such as hybrid graphics, high DPI displays and HDR might not work if the DXGI version required to support them is not on the system.

## Enhanced Features (Fork)

This fork by Inhahe adds category selection, preset management, and favorites. Built with the assistance of [Claude Code](https://claude.ai/code) using Claude Opus 4.6.

- **Recursive preset scanning** - Automatically finds `.milk` files in nested subdirectory structures (e.g., the Cream of the Crop collection organized by category).
- **Category selector** - Tri-state checkbox TreeView dialog for selecting which preset categories to include in shuffle. Selections persist across sessions.
- **Favorites system** - Add/remove presets to favorites via right-click menu. Manage Favorites dialog with scrollable list, double-click to load, remove button.
- **Random preset from category** - Right-click menu to load a random preset from a specific category.
- **Shuffle modes** - All presets, selected categories only, favorites only, or favorites in selected categories.
- **Configurable preset library** - Point the plugin at any directory containing `.milk` presets via right-click > Set Preset Library Folder.
- **Portable preset library path** - A preset folder inside the foobar2000 profile, or anywhere inside a portable foobar2000 installation, is saved relative to the profile, so the setting survives moving or copying the installation to another folder, drive or computer.
- **No-presets notice** - When the preset folder holds no presets, the visualization says so in its upper-right corner, points to Set Preset Library Folder, and names the folder it looked in; when the folder has presets but the shuffle mode excludes them all, it says that instead. It shows a plain waveform on black meanwhile.
- **Fullscreen hotkeys** - In fullscreen mode, keyboard shortcuts are shown in the right-click menu (Space, Backspace, R, F11, Shift+F11, etc.).
- **DX11 rendering fix** - Removed DX9-era half-texel offset that caused a thin unrendered border on right/bottom edges.

## Run Requirements and Installation

### One-File Install

[`install-milkdrop2-plus.cmd`](install-milkdrop2-plus.cmd) installs everything in one step, with nothing to build: the latest release of the plugin, the [original MilkDrop presets](https://github.com/projectM-visualizer/presets-milkdrop-original), the [Base Milkdrop Texture Pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack), and, if you want it, a Default UI layout with MilkDrop on the right and a spectrum, an oscilloscope, a spectrogram and the playlists on the left.

1. Install [foobar2000](https://www.foobar2000.org/download) v2.x (64-bit), and close it.
2. Download [`install-milkdrop2-plus.cmd`](https://github.com/inhahe/milkdrop2-plus/releases/latest/download/install-milkdrop2-plus.cmd) (it comes with each release).
3. Double-click it. It finds foobar2000 and asks before installing there (or lets you pick `foobar2000.exe`), then asks whether to install the layout. If Windows says "Windows protected your PC", click More info, then Run anyway.
4. Start foobar2000.

From a command prompt: `install-milkdrop2-plus.cmd "C:\foobar2000"`, with `-Layout` or `-NoLayout` to answer the layout question in advance. Everything goes into foobar2000's profile folder, and whatever it replaces (the layout, the preset folder setting, an earlier build of the plugin) is first saved in `<profile>\milkdrop2-plus-install-backup\`. Run it again to update the plugin and add any new presets and textures; those already there are kept, edited or not. Its other options (`-LayoutFrom`, `-Force`) are described at the top of the file.

### Install from Source

1. Download [foobar2000](https://www.foobar2000.org/download) v2.x (64-bit) and install.
2. Clone this repo with presets included:
   ```
   git clone --recursive https://github.com/inhahe/milkdrop2-plus.git
   ```
   The `--recursive` flag downloads the [Cream of the Crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop) preset collection (~10,000 presets) as a submodule. If you clone without `--recursive`, you can fetch the presets later with `git submodule update --init`, or `install.bat` will offer to download them for you.
3. Build it (Release x64; see [BUILDING.md](BUILDING.md)).
4. Run `install.bat` to install the plugin and presets into foobar2000.

### Manual Install

`<profile>` below is foobar2000's profile folder: `<foobar2000>\profile` in a portable installation, otherwise `%APPDATA%\foobar2000-v2` (`install.bat` works this out the same way).

1. Copy `foo_milkdrop2_plus.dll` (from `Bin\x64\Release\`) to: `<profile>\user-components-x64\foo_milkdrop2_plus\`
2. Copy shader files (`*.fx`) from `external\winamp\data\` to: `<profile>\user-components-x64\foo_milkdrop2_plus\data\`
3. Download presets (see below) and extract into: `<profile>\milkdrop2\presets\`, or anywhere else and choose that folder with right-click > Set Preset Library Folder.
4. Optionally, install the texture pack (see [Textures](#textures)).
5. Start foobar2000, enable Layout Editing (View > Layout > Enable Layout Editing), right-click a panel, and add MilkDrop from the Visualization category.

### Directory Structure

```
<profile>\
    user-components-x64\
        foo_milkdrop2_plus\
            foo_milkdrop2_plus.dll  <- the plugin
            data\
                comp_vs.fx         <- shader files (required!)
                comp_ps.fx
                warp_vs.fx
                warp_ps.fx
                blur_vs.fx
                blur1_ps.fx
                blur2_ps.fx
                include.fx
    milkdrop2\
        presets\                    <- .milk preset files go here (the default preset folder)
            Fractal\               <- category subdirectories are supported
                Blobby\            <- nested subdirectories work too
                    preset.milk
            Geometric\
                ...
        textures\                   <- textures that presets use (see Textures below)
    browser_config.ini             <- auto-generated settings, including the preset folder
    favorites.ini                  <- auto-generated favorites list
```

- Sprite images can also be added to `<foobar2000 profile folder>\milkdrop2\textures` and referenced in their accompanying configuration file `<foobar2000 profile folder>\milkdrop2\milk2_img.ini`.
- Custom messages can be entered into `<foobar2000 profile folder>\milkdrop2\milk2_msg.ini`.

### Presets

The preset files define the visualizations via pixel shaders, equations and parameters.

The foobar2000 component is not packaged with any presets. Some presets enjoyed by Ryan Geiss (the MilkDrop 2 author) can be downloaded from his [website](https://www.geisswerks.com/milkdrop/). In addition, the projectM repository archives several preset and texture collections:

- [Cream of the Crop Pack](https://github.com/projectM-visualizer/presets-cream-of-the-crop): A collection of about 10,000 presets compiled by Jason Fletcher.
- [Base Milkdrop Texture Pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack): Recommended for use with _any_ preset pack!
- [Milkdrop 2 Presets](https://github.com/projectM-visualizer/presets-milkdrop-original): The original preset collection shipped with Milkdrop and Winamp.

### Textures

Many presets draw with texture images that are not part of the preset files. Without them, those presets get black where the texture should be. To install the [Base Milkdrop Texture Pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack):

1. Download it (Code > Download ZIP on its GitHub page, or `git clone https://github.com/projectM-visualizer/presets-milkdrop-texture-pack.git`).
2. Copy the image files from its `textures` folder into `<foobar2000 profile folder>\milkdrop2\textures\` (in a portable installation, `<foobar2000>\profile\milkdrop2\textures\`; otherwise usually `%APPDATA%\foobar2000-v2\milkdrop2\textures\`). Create the folder if it does not exist.

The plugin also looks for a texture at the top level of the preset folder (not in its subfolders).

## Repository Notes

The build assumes the following directory structure:

```text
 .github\ -> contains the upstream project's continuous integration build pipeline, kept disabled for reference.
 Bin\ -> generated by Visual Studio to contain the final DLLs and PDBs.
 component\ -> generated by the packaging script to layout the component prior to archiving and compression.
 data\ -> contains static files to be included in the component package.
     pdbs\ -> generated by the packaging script and contains the PDBs for each release.
     foo_vis_milk2-*.fb2k-component -> generated by the packaging script and are the packaged component releases.
 external\ -> contains external library dependencies.
     dxtk\ -> the DirectX Tool Kit (DirectXTK) sources the component uses, built into vis_milk2.
     eel2\ -> contains the Nullsoft Expression Evaluator Library (NS-EEL).
     foobar2000\ -> contains most of the foobar2000 SDK download.
         component_client\ -> from the foobar2000 SDK, generates the DLL entrypoint function for the component.
         helpers\ -> from the foobar2000 SDK, constains a library of various helper code for the component.
         sdk\ -> from the foobar2000 SDK, contains declarations of services and various service-specific helper code.
         shared\ -> from the foobar2000 SDK, contains the various utility code used by foobar2000 components.
     libppui\ -> from the foobar2000 SDK, contains a library of helper code, mainly Windows UI code.
     nu\ -> contains the Nullsoft utilities.
     pfc\ -> from the foobar2000 SDK, a class library used by the foobar2000 SDK.
     projectm-eval\ -> from GitHub, contains the projectM expression evaluation library.
     winamp\ -> contains header files, shader files and documentation from the Winamp release.
         data\ -> contains the Winamp pixel and vertex shaders.
         docs\ -> contains the MilkDrop 2 documentation.
     wtl.*\ -> from NuGet, the Windows Template Library (WTL).
 foo_vis_milk2\ -> contains the foobar2000 component code.
 Obj\ -> generated by Visual Studio to contain the intermediate build object files.
 tools\ -> contains the packaging and formatting scripts.
 vis_milk2\ -> contains the MilkDrop 2 visualization library code.
```

### Build Notes

- Removed `/d2notypeopt` Visual C++ compiler option as it is applied by default on Visual Studio 2019 version 16.6 and later. ([1](https://hydrogenaud.io/index.php/topic,108411.0.html), [2](https://developercommunity.visualstudio.com/t/invalid-function-call-de-virtualization/1125222))
- Built all targets using v143 Platform Toolset for as `/arch` being "Not set" should default to `/arch:SSE2` on Visual Studio 2022 version 17.10 and later. ([1](https://hydrogenaud.io/index.php/topic,125795.0.html), [2](https://developercommunity.visualstudio.com/t/Cannot-disable-AVX-and-AVX2-in-VS-2022/10497078))

## MilkDrop 2

MilkDrop 2 (`vis_milk2`) is a music visualizer - a "plug-in" to the Winamp music player. The changes to the [MilkDrop 2 source code release](https://sourceforge.net/projects/milkdrop2/) from 5/13/13 (version 2.25c) include:

- Porting VMS from DirectX 9 to Direct X 11.1. DirectX 11.1 is Direct3D 11.1, DXGI 1.6, Direct2D 1.1, and DirectWrite 1.1.
- Porting text layout and rendering from D3DX9 and GDI to DirectWrite and Direct2D, respectively.
- Building DLL with Visual Studio 2022 (v143) Platform Toolset.
- Minor bug and typo fixing so that the plug-in can be used in Winamp and foobar2000 music players without crashing.
- Fixing of string resources to flow consistently with Segoe UI spacing and sizing.
- Minor cleaning and updating of configuration panel to match functioning features and UI modifications.
- Developer experience improvements, such as:
  - Updated dependencies to latest available versions.
  - Refactored EEL2 and DirectXTK into separate projects.
  - PCH and multiprocessor compile enabled for fast builds.
  - Buildable with C++20 compiler, including the address sanitizer and fuzzer. Builds clean using `/W4` (level 1,2,3,4 compiler warnings) and `/WX` (treat compiler warnings as errors) build options.
  - All character string and memory manipulation functions migrated to use their respective secure CRT versions.
  - Several utility functions and container classes replaced with their STL equivalents.
  - Enabled and added x86 and ARM 64-bit builds (x64, ARM64 and ARM64EC platforms) in addition to the upgraded x86 32-bit (Win32 platform) one. The 64-bit DLLs are not as extensively tested since most music players are still 32-bit applications.
  - Added a minimal unit test with associated infrastructure and some API service mock classes to test DLL initialization.
  - Added CI pipeline (GitHub Actions).
  - Enforced more consistent formatting (with ClangFormat), line endings, and file encodings. Important notes:
    - `*.rc` encoding was kept as "Western European (Windows) - Codepage 1252".
    - `*.vcxproj/*.sln` encoding was kept as "Unicode (UTF-8 with signature/BOM) - Codepage 65001".
    - All other text files use "Unicode (UTF-8 without signature) - Codepage 65001". All text files use CRLF line endings.
  - Updated comments and consolidated documentation.