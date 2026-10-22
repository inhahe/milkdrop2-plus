# Known issues and technical debt

Bugs and debt found but not yet fixed, each with where it lives and what the fix looks like.

## Fixed-size path buffers can end the process

**Where:** `vis_milk2/plugin.cpp` and `foo_vis_milk2/keyboard.cpp`, wherever a path is assembled into a `wchar_t[MAX_PATH]` or `[512]` array with `wcscpy_s`, `wcscat_s` or `swprintf_s`: preset loading inside `LoadPreset`, the delete and rename prompts, and others.

**What happens:** with the statically linked runtime, a `_s` function given a result longer than its buffer calls the invalid-parameter handler, which ends foobar2000. The preset folder itself is now bounded (`MAX_PRESET_DIR_LENGTH`), but a folder near that limit plus a long relative preset name still overflows.

**Fix:** build these paths as `std::wstring` and pass `c_str()` on, as `CPlugin::GetPresetPath` and the texture lookup (`ForEachTextureCandidate`) now do.

## The UI thread reads the preset list without the lock

**Where:** `foo_vis_milk2/ui_element.cpp` and `keyboard.cpp` read `g_plugin.m_presets`, `m_nPresets`, `m_nPresetFilesFound` and the other scan results directly, while `__UpdatePresetList` replaces them under `g_cs` on its own thread.

**What happens:** a right-click or a key press during a rescan can see a list and a count from different scans, or a list being moved from.

**Fix:** give `CPlugin` accessors that take `g_cs` and return copies (or run the UI's preset operations under the lock), and use them from the component.

## Menu and key commands change the engine on the UI thread while the render thread draws

**Where:** `foo_vis_milk2/ui_element.cpp` (Next, Previous and Random Preset, a category, a favorite, the preset browser) and `keyboard.cpp` (the same keys, and the pixel shader upgrade) call `g_plugin.LoadPreset`, `NextPreset`, `PrevPreset`, `LoadRandomPreset` and `LoadShaders` on the UI thread, holding neither `s_cs` nor `g_cs`; the render thread draws on a thread-pool timer (`TIMER_TP`) under both.

**What happens:** a preset loaded from the menu or a key swaps `m_pState`, releases and replaces `m_shaders`, and, with no blend time, compiles shaders and binds textures (`m_textures`, `m_missingTextures`) while the render thread may be drawing with them or running `LoadPresetTick`: a crash or a garbled frame is possible, though rare in practice. Upper-right messages are already safe from any thread (they are queued: `AddError`), and so is a change of texture folder (`SetTextureDir` only marks it).

**Fix:** run these commands on the render thread: queue them, as `AddError` does, and carry them out at the start of `MilkDropRenderFrame`, under `g_cs`. Taking `g_cs` on the UI thread instead would block it behind a frame, and the render thread sends the main window messages (`GetWinampSongTitle`, with a 100 ms timeout; `ConsoleMessage`, without one), which a blocked UI thread cannot answer.

## UI-thread calls that take a render lock wait out a frame, and stall the song title

**Where:** `CPlugin::SetPresetDir`, `GetPresetPath` and `GetScannedPresetDir` take `g_cs`, and `ui_element.cpp` takes `s_cs` (configuration changes, window swaps), on the UI thread; the render thread holds both for the whole frame.

**What happens:** during the frame the render thread asks the main window for the song title (`GetWinampSongTitle`: two `SendMessageTimeout` calls, 100 ms each). A UI thread waiting on the lock cannot answer them, so it stalls for about 200 ms, and the title reads empty for that frame, which can restart the song-title animation when it comes back. (`SetTextureDir` avoids this with its own lock, held only to copy the folder.)

**Fix:** have the UI thread hand the song title to the engine (it has it from foobar2000's playback callbacks) instead of the render thread asking for it; and publish what the UI reads from a scan under a lock the render thread holds only to copy it.

## The Columns UI element has no context menu

**Where:** `foo_vis_milk2/cui_element.cpp`, compiled in whenever the `external/columns_ui-sdk` submodule is checked out, as in the release build and any recursive clone.

**What happens:** Set Preset Library Folder, Set Texture Folder, the shuffle modes and favorites exist only in the Default UI element's context menu, which the no-presets notice and the missing-texture hint also point to. In Columns UI the texture folder can be set only as the Texture directory advanced setting, which is read when the element is created or the preferences are applied.

**Fix:** share the Default UI element's menu with the Columns UI panel, or leave the element out of the build until it is supported.

## The scan cache records default ratings for filtered-out presets

**Where:** `__UpdatePresetList` in `vis_milk2/plugin.cpp`, step 4.

**What happens:** the cache is written after the shuffle mode's filters, so presets they excluded are cached with the default rating 3.0 instead of their own; after the filters change, those presets keep that rating until the folder's files change and force a full scan.

**Fix:** read the ratings of all files before filtering, or write the cache from the unfiltered pass.

## install.bat can leave two builds of the component installed

**Where:** `install.bat`, which copies a local build into `user-components-x64\foo_milkdrop2_plus\`.

**What happens:** it leaves a build under the component's older names (`foo_vis_milk2`, `milkdrop2_plus`) in place, and all of them register the same Default UI element, so foobar2000 loads two components that claim it. `install-milkdrop2-plus.cmd` moves those aside, and `uninstall.bat` removes all three, but each script keeps its own copy of what the component is called and where it goes, and they have drifted apart before.

**Fix:** give `install-milkdrop2-plus.cmd` an option to install a local build (`Bin\x64\Release` instead of the latest release), and make `install.bat` a thin wrapper around it, so that the component's names, the profile's layout and the running check live in one place.
