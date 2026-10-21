# MilkDrop 2 Plus: design

How the component is put together, for whoever changes it next. [README](README.md) is the user's view; this is the maintainer's.

## Parts

| Part | What it is |
|---|---|
| `foo_vis_milk2/` | The foobar2000 component (`foo_milkdrop2_plus.dll`): the Default UI element (`ui_element.cpp`, which also `#include`s `keyboard.cpp` and the Columns UI element `cui_element.cpp`, compiled out while `HAS_COLUMNS_UI` is off), preferences and advanced settings (`preferences.cpp`, `config.h`), the preset browser and categories (`preset_browser.*`), favorites (`favorites_manager.*`), and how saved paths are stored (`stored_path.*`). |
| `vis_milk2/` | The MilkDrop 2 engine, a static library: `CPlugin` (`plugin.cpp`: presets, menus, text overlay), its shell (`pluginshell.cpp`: window, timing, drawing order), rendering (`milkdropfs.cpp`), preset state and equations (`state.cpp`), a Direct3D 9-style shim over Direct3D 11 (`d3d11shim.cpp`), and Direct2D/DirectWrite text (`textmgr.cpp`). |
| `external/` | Dependencies. `eel2/`, `nu/`, `winamp/data/` (the shader files shipped in the component's `data` folder), `dxtk/` (the DirectXTK sources used) are in the repository; the foobar2000 SDK, PFC, libPPUI, projectM-EEL and WTL are fetched and patched as BUILDING.md describes. |
| `test/` | CppUnitTest tests, run with `vstest.console.exe` on `Bin\<platform>\<config>\test.dll`. |

## Rendering

Each frame (`CPlugin::RenderFrame`, `milkdropfs.cpp`) works on two textures that swap roles every frame, VS0 (last frame) and VS1 (this frame):

1. Motion vectors are drawn into VS0.
2. The warp pass draws VS0, warped and faded by the preset, into VS1. With no preset loaded (the state's description is `INVALID_PRESET_DESC`), VS1 is cleared to black instead: there is no warp to fade old content, and without the clear each texture keeps everything ever drawn into it and the picture flickers between the two.
3. Blur passes, then custom shapes, custom waves, the main wave and sprites are drawn into VS1. A song title animation that has just ended is stamped into VS1 here.
4. The composite pass draws VS1 to the back buffer; the running song title animation and user sprites go on top.
5. VS0 and VS1 swap.

The shim keeps one current pixel shader; a draw that needs a particular one (for instance `ShowSongTitleAnim`, which needs the textured shader 0) must select it, because the previous draw may have left an untextured one.

Text (preset name, notices, menus) is retained-mode: each line is a `TextElement` registered with `CTextManager`, laid out by `MilkDropRenderUI` each frame, and hidden and unregistered when no longer shown. An upper-right box is one line high, so multi-line notices use one element per line.

## Presets

- The preset folder is scanned recursively on a background thread (`__UpdatePresetList`), which keeps a cache, `preset_cache.dat`, in the preset folder. The first preset is loaded by `MilkDropRenderFrame` once the scan has finished.
- The scan thread also records, under `g_cs`, the folder it read (`m_scannedPresetDir`) and how many `.milk` files it found there before the shuffle mode's category and favorites filters (`m_nPresetFilesFound`). After initialization, which creates `g_cs`, the folder to scan is changed only through `CPlugin::SetPresetDir`, which takes the lock and refuses a path that does not fit.
- With no presets, the engine keeps the default state (`CState::Default`): a plain waveform on black. `MilkDropRenderUI` meanwhile shows a notice in the upper-right corner: with no files, "No presets found", the command to choose a folder (Set Preset Library Folder, also at the top of the right-click menu) and the folder it looked in; with files that the shuffle mode excludes, that and how to change it. Its advice is the Default UI element's context menu.
- Favorites are stored as paths relative to the preset folder, so they survive moving the folder.

## Settings and files

| What | Where |
|---|---|
| Most settings | foobar2000's configuration (`milk2.*` advanced settings and the preferences page), in `config.sqlite`. |
| Preset folder | `PresetLibraryPath` in `<profile>\browser_config.ini`, written by Set Preset Library Folder; it overrides the `milk2.szPresetDir` advanced setting when the UI element is created. `milk2_config::build` writes the folder in use back to `milk2.szPresetDir` whenever foobar2000 saves the element's configuration. |
| Shuffle mode, categories | `<profile>\browser_config.ini` |
| Favorites | `<profile>\favorites.ini` |
| Textures, sprites, messages | `<profile>\milkdrop2\textures\`, `milk2_img.ini`, `milk2_msg.ini`. A texture is looked up in `textures\`, then in the preset folder. |

Both stores of the preset folder go through `milk2_config::preset_dir_to_stored`/`preset_dir_from_stored`, and so `ToStoredPath`/`FromStoredPath` (`stored_path.h`): a folder inside the profile, or anywhere inside a portable installation (the profile's parent), is saved relative to the profile, so a moved or copied installation keeps finding it; anything else stays absolute, and absolute values saved by older versions still load. A loaded path that would not fit the engine's `MAX_PATH` buffers counts as unset. When the folder is missing, `FindValidPresetDir` falls back to `<profile>\milkdrop2\presets\`, then `<profile>\milkdrop2\`, then the profile.

## Build

- `milkdrop2-plus.sln`, Visual Studio 2026 (toolset v145). The release package is the x64 Release DLL plus `data\` (`external\winamp\data\*.fx` and `vis_milk2\help.bin`), zipped as `milkdrop2-plus.fb2k-component`.
- `Directory.Build.props` (through `ForceImportBeforeCppProps` and `toolset.props`) sets toolset v145, and `Directory.Build.targets` the statically linked C/C++ runtime, for every project in the tree, including the foobar2000 SDK projects whose project files are not ours. foobar2000 loads one copy of the DLL runtime for the whole process, from its own folder, and it can be older than our compiler (foobar2000 2.26 ships 14.44; v145 code needs 14.50 or later); a runtime linked into the DLL does not depend on it. Every library linked in must use the same runtime, which is why DirectXTK is built from source (`external/dxtk`) rather than taken from NuGet, whose libraries use the DLL runtime.
- v145 code needs Windows 10 or later.
