# Known issues and technical debt

Bugs and debt found but not yet fixed, each with where it lives and what the fix looks like.

## Fixed-size path buffers can end the process

**Where:** `vis_milk2/plugin.cpp` and `foo_vis_milk2/keyboard.cpp`, wherever a path is assembled into a `wchar_t[MAX_PATH]` or `[512]` array with `wcscpy_s`, `wcscat_s` or `swprintf_s`: the texture lookup (`swprintf_s(szFilename, L"%s%s.%s", ...)` in the shader parameter setup), preset loading inside `LoadPreset`, the delete and rename prompts, and others.

**What happens:** with the statically linked runtime, a `_s` function given a result longer than its buffer calls the invalid-parameter handler, which ends foobar2000. The preset folder itself is now bounded (`MAX_PRESET_DIR_LENGTH`), but a folder near that limit plus a long relative preset or texture name still overflows.

**Fix:** build these paths as `std::wstring` and pass `c_str()` on, as `CPlugin::GetPresetPath` now does for preset loading.

## The UI thread reads the preset list without the lock

**Where:** `foo_vis_milk2/ui_element.cpp` and `keyboard.cpp` read `g_plugin.m_presets`, `m_nPresets`, `m_nPresetFilesFound` and the other scan results directly, while `__UpdatePresetList` replaces them under `g_cs` on its own thread.

**What happens:** a right-click or a key press during a rescan can see a list and a count from different scans, or a list being moved from.

**Fix:** give `CPlugin` accessors that take `g_cs` and return copies (or run the UI's preset operations under the lock), and use them from the component.

## The Columns UI element has no context menu

**Where:** `foo_vis_milk2/cui_element.cpp`, compiled into the release build because `external/columns_ui-sdk` is present.

**What happens:** Set Preset Library Folder, the shuffle modes and favorites exist only in the Default UI element's context menu, which the no-presets notice also points to.

**Fix:** share the Default UI element's menu with the Columns UI panel, or leave the element out of the build until it is supported.

## The scan cache records default ratings for filtered-out presets

**Where:** `__UpdatePresetList` in `vis_milk2/plugin.cpp`, step 4.

**What happens:** the cache is written after the shuffle mode's filters, so presets they excluded are cached with the default rating 3.0 instead of their own; after the filters change, those presets keep that rating until the folder's files change and force a full scan.

**Fix:** read the ratings of all files before filtering, or write the cache from the unfiltered pass.
