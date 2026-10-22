/*
 * album_art_files.cpp - Which image files the album art may come from.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

// Self-contained, without the component's precompiled header, so the tests can build it alone.
#include "album_art_files.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{
std::wstring Wide(const std::string& text)
{
    std::wstring wide;
    if (const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0); n > 0)
    {
        wide.resize(static_cast<size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), n);
    }
    return wide;
}

// Whether `a` and `b` are the same name, or the same path, but for case.
bool SameName(const std::string& a, const std::string& b)
{
    const std::wstring wa = Wide(a), wb = Wide(b);
    return CompareStringOrdinal(wa.data(), static_cast<int>(wa.size()), wb.data(), static_cast<int>(wb.size()), TRUE) == CSTR_EQUAL;
}

// The name of the file at `path`, without its folder or its extension.
std::string BaseName(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    if (const size_t dot = name.rfind('.'); dot != std::string::npos && dot > 0)
        name.resize(dot);
    return name;
}
} // namespace

bool IsAlbumImage(const std::string& image, const std::string& track, const std::string& album, const std::vector<std::string>& folderTracks,
                  const std::function<std::optional<std::string>(const std::string&)>& albumOf)
{
    const std::string name = BaseName(image);
    if (SameName(name, BaseName(track)) || (!album.empty() && SameName(name, album)))
        return true;
    for (const std::string& other : folderTracks)
    {
        if (SameName(other, track))
            continue;
        if (album.empty())
            return false;
        if (const std::optional<std::string> otherAlbum = albumOf(other); otherAlbum && !SameName(*otherAlbum, album))
            return false;
    }
    return true;
}
