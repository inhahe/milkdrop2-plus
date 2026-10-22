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

**Where:** `foo_vis_milk2/ui_element.cpp` (Next, Previous and Random Preset, a category, a favorite, the preset browser) and `keyboard.cpp` (the same keys) call `g_plugin.LoadPreset`, `NextPreset`, `PrevPreset` and `LoadRandomPreset` on the UI thread, holding neither `s_cs` nor `g_cs`; the render thread draws on a thread-pool timer (`TIMER_TP`) under both.

**What happens:** a preset loaded from the menu or a key swaps `m_pState`, releases and replaces `m_shaders`, and, with no blend time, compiles shaders and binds textures (`m_textures`, each shader's missing-texture list, the random-texture list) while the render thread may be drawing with them, running `LoadPresetTick`, or reading those lists for the missing-texture message: a crash, a garbled frame or a wrong name in that message is possible, though rare in practice. (A blended load, the usual kind, only reads the preset there and compiles on the render thread.) Already safe from any thread: upper-right messages (`AddError` queues them), a change of texture folder (`SetTextureDir` only marks it), and the shader editor and the pixel shader upgrade (`m_shaderRequests`, which the render thread carries out).

**Fix:** run these commands on the render thread too: queue them, as `m_shaderRequests` does, and carry them out at the start of `MilkDropRenderFrame`, under `g_cs`. (Taking `g_cs` on the UI thread instead would freeze it for a shader compile.)

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
