/*
 * stored_path.cpp - How directory paths are saved in the component's configuration.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

// Self-contained, without the component's precompiled header, so the tests can build it alone.
#include "stored_path.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>
#include <vector>

namespace
{
// The components of `p` after normalization ("D:", "\", "a", "b"), without the empty one that a
// trailing separator leaves.
std::vector<std::wstring> Components(const std::wstring& p)
{
    std::vector<std::wstring> parts;
    for (const auto& part : std::filesystem::path(p).lexically_normal())
    {
        if (!part.empty())
            parts.push_back(part.wstring());
    }
    return parts;
}

// Whether `base` is a leading part of `parts`, ignoring case as Windows paths do.
bool StartsWith(const std::vector<std::wstring>& parts, const std::vector<std::wstring>& base)
{
    if (base.empty() || base.size() > parts.size())
        return false;
    for (size_t i = 0; i < base.size(); ++i)
    {
        if (CompareStringOrdinal(parts[i].c_str(), -1, base[i].c_str(), -1, TRUE) != CSTR_EQUAL)
            return false;
    }
    return true;
}

bool EndsWithSeparator(const std::wstring& p)
{
    return !p.empty() && (p.back() == L'\\' || p.back() == L'/');
}
} // namespace

std::wstring ToStoredPath(const std::wstring& path, const std::wstring& profileDir, bool bPortable)
{
    if (path.empty() || profileDir.empty() || !std::filesystem::path(path).is_absolute())
        return path;

    const std::vector<std::wstring> parts = Components(path);
    const std::vector<std::wstring> profile = Components(profileDir);

    std::wstring relative;
    size_t first; // index in `parts` of the first component below the folder it is relative to
    if (StartsWith(parts, profile))
    {
        first = profile.size();
    }
    else if (bPortable && profile.size() > 2 && StartsWith(parts, std::vector<std::wstring>(profile.begin(), profile.end() - 1)))
    {
        // Inside the foobar2000 folder of a portable installation, the profile's parent. The size
        // check requires the profile to be a folder below a root ("D:", "\", "profile"), so that
        // its parent is a folder too.
        relative = L"..\\";
        first = profile.size() - 1;
    }
    else
    {
        return path;
    }

    for (size_t i = first; i < parts.size(); ++i)
    {
        relative += parts[i];
        if (i + 1 < parts.size() || EndsWithSeparator(path))
            relative += L'\\';
    }
    if (relative.empty())
        relative = L".\\"; // the profile folder itself; an empty value means "not set"
    return relative;
}

std::wstring FromStoredPath(const std::wstring& stored, const std::wstring& profileDir)
{
    if (stored.empty() || std::filesystem::path(stored).is_absolute())
        return stored;
    return (std::filesystem::path(profileDir) / stored).lexically_normal().wstring();
}
