# MilkDrop 2 Plus: design

How the component is put together, for whoever changes it next. [README](README.md) is the user's view; this is the maintainer's.

## Parts

| Part | What it is |
|---|---|
| `foo_vis_milk2/` | The foobar2000 component (`foo_milkdrop2_plus.dll`): the Default UI element (`ui_element.cpp`, which also `#include`s `keyboard.cpp` and the Columns UI element `cui_element.cpp`, compiled in whenever `external/columns_ui-sdk` is present, as in the release build, which sets `HAS_COLUMNS_UI`; that element has no context menu), preferences and advanced settings (`preferences.cpp`, `config.h`), the preset browser and categories (`preset_browser.*`), favorites (`favorites_manager.*`), and how saved paths are stored (`stored_path.*`). |
| `vis_milk2/` | The MilkDrop 2 engine, a static library: `CPlugin` (`plugin.cpp`: presets, menus, text overlay), its shell (`pluginshell.cpp`: window, timing, drawing order), rendering (`milkdropfs.cpp`), preset state and equations (`state.cpp`), a Direct3D 9-style shim over Direct3D 11 (`d3d11shim.cpp`), Direct2D/DirectWrite text (`textmgr.cpp`), and the component's own text files (`utf8_file.*`). |
| `external/` | Dependencies. `eel2/`, `nu/`, `winamp/data/` (the shader files shipped in the component's `data` folder), `dxtk/` (the DirectXTK sources used) are in the repository; the foobar2000 SDK, PFC, libPPUI, projectM-EEL and WTL are fetched and patched as BUILDING.md describes. |
| `test/` | CppUnitTest tests, run with `vstest.console.exe` on `Bin\<platform>\<config>\test.dll`. |
| `install-milkdrop2-plus.cmd` | The one-file installer for users, which installs a release: see [One-file installer](#one-file-installer). |

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
- With the list, the scan thread records under `g_cs` the folder it read (`m_scannedPresetDir`), how many `.milk` files it found there (`m_nPresetFilesFound`) and how many of them the shuffle mode's category and favorites filters keep (`m_nPresetFilesInShuffle`); the list then drops presets that need a newer pixel shader model than allowed. The names in `m_presets` are relative to `m_scannedPresetDir`, so preset paths are built from it (`CPlugin::GetPresetPath`, which takes `g_cs`), not from `m_szPresetDir`, which already names the next folder while its scan runs.
- The folder to scan is changed only through `CPlugin::SetPresetDir`, which refuses a path longer than `MAX_PRESET_DIR_LENGTH` (a scan formats it with `*.*` into a `MAX_PATH` buffer) and takes `g_cs` while that lock exists (from engine initialization to cleanup; before or after, no other thread uses the folder). Cleanup stops the scan thread, which checks for the request at every file, before deleting the lock.
- With no presets, the engine keeps the default state (`CState::Default`): a plain waveform on black. `MilkDropRenderUI` meanwhile shows a notice in the upper-right corner, and the right-click menu says the same at its top: with no files, "No presets found", the command to choose a folder (Set Preset Library Folder) and the folder it looked in; with files that the shuffle mode excludes, that and how to change it; with files kept but none loadable, that. Its advice is the Default UI element's context menu.
- Favorites are stored as paths relative to the preset folder, so they survive moving the folder.

## Settings and files

| What | Where |
|---|---|
| Most settings | foobar2000's configuration (`milk2.*` advanced settings and the preferences page), in `config.sqlite`. |
| Preset folder | `PresetLibraryPath` in `<profile>\browser_config.ini` (read for both elements by `ReadKeyValueFile`), written by Set Preset Library Folder; it overrides the `milk2.szPresetDir` advanced setting when the UI element is created. `milk2_config::build` writes the folder in use back to `milk2.szPresetDir` whenever foobar2000 saves the element's configuration. |
| Shuffle mode, categories | `<profile>\browser_config.ini` |
| Favorites | `<profile>\favorites.ini` |
| Preset scan cache | `preset_cache.dat` in the preset folder |
| Textures, sprites, messages | `<profile>\milkdrop2\textures\`, `milk2_img.ini`, `milk2_msg.ini`. A texture is looked up in `textures\`, then in the preset folder. |

browser_config.ini, favorites.ini and preset_cache.dat are UTF-8, through `WriteUtf8TextFile`/`ReadTextFile`/`ReadKeyValueFile` (`utf8_file.h`). Writing goes through a temporary file, so a failure leaves the old file whole; reading falls back to Latin-1 line by line, so files from earlier versions still read and one bad line spoils only itself.

Both stores of the preset folder go through `milk2_config::preset_dir_to_stored`/`preset_dir_from_stored`, and so `ToStoredPath`/`FromStoredPath` (`stored_path.h`): a folder inside the profile, or anywhere inside a portable installation (the profile's parent), is saved relative to the profile, so a moved or copied installation keeps finding it; anything else stays absolute, and absolute values saved by older versions still load. A loaded path gets its trailing backslash there, and one longer than `MAX_PRESET_DIR_LENGTH` is not used; the Default UI element saves such a `PresetLibraryPath` back as it was, so that moving the installation back restores it. When the folder is missing, `FindValidPresetDir` falls back to `<profile>\milkdrop2\presets\`, then `<profile>\milkdrop2\`, then the profile.

## One-file installer

`install-milkdrop2-plus.cmd` installs the latest release, the original MilkDrop presets and the texture pack, and offers a layout, for someone who has only that file and builds nothing. It is a batch file whose first lines PowerShell reads as a block comment (`<# : ... #>`): cmd runs them, copies the file to a temporary `.ps1` (the only kind of file PowerShell runs) and runs that in Windows PowerShell with `-ExecutionPolicy Bypass`. The repository keeps it byte for byte (`-text` in `.gitattributes`), so that GitHub's raw download has CRLF line endings, and its batch part has no labels or blocks, which cmd misreads in a file with LF endings.

It relies on these contracts of the component, so a change to any of them must change it too:

- The profile: `<foobar2000>\profile` when `portable_mode_enabled` exists beside foobar2000.exe, `%APPDATA%\foobar2000-v2` otherwise; components in `user-components-x64\<DLL name>\`.
- Every name the component's DLL has had (`$ComponentNames`: `foo_vis_milk2` until v0.2.1, `milkdrop2_plus` from an earlier `install.bat`, `foo_milkdrop2_plus` since; `uninstall.bat` lists the same three). All register the Default UI element `guid_milk2`, so it moves the others aside; it also looks for that GUID in a layout.
- A running foobar2000 keeps its profile's `config.sqlite` open, which is how it tells that foobar2000 is running with the profile it is about to change.
- `PresetLibraryPath` in `browser_config.ini`, which it sets to `milkdrop2\presets\`, relative to the profile (`FromStoredPath`, v0.3.0 and later), keeping the file's other lines and their encodings as [Settings and files](#settings-and-files) describes.
- Textures in `<profile>\milkdrop2\textures\`.
- The release asset: one `*.fb2k-component`, a zip with one DLL at its top.

The layout it offers is a Default UI `theme.fth`, embedded in base64 at the end of the file and taken from a working installation. The MilkDrop element's configuration in a layout is only the `MILK` sentinel and the version (`milk2_config::build`), with no paths, so the layout works in any profile. To offer another, embed its `theme.fth` and change `$EmbeddedLayoutDescription`.

## Build

- `milkdrop2-plus.sln`, Visual Studio 2026 (toolset v145). The release package is the x64 Release DLL plus `data\` (`external\winamp\data\*.fx` and `vis_milk2\help.bin`), zipped as `milkdrop2-plus.fb2k-component`. Each release also carries `install-milkdrop2-plus.cmd` as an asset (`gh release upload <tag> install-milkdrop2-plus.cmd`): the README's One-File Install links to `releases/latest/download/install-milkdrop2-plus.cmd`, which a release without it breaks. After a change to the installer, upload it to the latest release again, with `--clobber`, or that link keeps serving the old one.
- `Directory.Build.props` (through `ForceImportBeforeCppProps` and `toolset.props`) sets toolset v145, and `Directory.Build.targets` the statically linked C/C++ runtime, for every project in the tree, including the foobar2000 SDK projects whose project files are not ours; our own project files set neither, so these are the only place to change them. foobar2000 loads one copy of the DLL runtime for the whole process, from its own folder, and it can be older than our compiler (foobar2000 2.26 ships 14.44; v145 code needs 14.50 or later); a runtime linked into the DLL does not depend on it. Every library linked in must use the same runtime, which is why DirectXTK is built from source (`external/dxtk`) rather than taken from NuGet, whose libraries use the DLL runtime.
- v145 code needs Windows 10 or later.
