/*
 * stored_path.h - How directory paths are saved in the component's configuration.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

// The preset library folder is saved relative to the foobar2000 profile folder when it lies inside
// that folder or, in a portable installation, anywhere inside the foobar2000 folder that holds the
// profile. A profile or a portable installation that moves to another folder, drive or computer
// then still finds it. A folder anywhere else is saved as an absolute path.

// `path`: the absolute directory path to save.
// `profileDir`: the foobar2000 profile folder.
// `bPortable`: whether foobar2000 runs as a portable installation, whose profile folder is the
//              "profile" folder inside the foobar2000 folder.
// Returns the text to save: `path` relative to `profileDir` ("milkdrop2\presets\",
// "..\presets\", ".\") where the rule above allows it, otherwise `path` unchanged.
std::wstring ToStoredPath(const std::wstring& path, const std::wstring& profileDir, bool bPortable);

// `stored`: a path as saved, relative (by `ToStoredPath()`) or absolute (as every path was saved
//           before, and still is outside the profile and the portable installation).
// `profileDir`: the foobar2000 profile folder.
// Returns the absolute path; a relative one is resolved against `profileDir`. A trailing backslash
// is kept, and an empty `stored` stays empty.
std::wstring FromStoredPath(const std::wstring& stored, const std::wstring& profileDir);
