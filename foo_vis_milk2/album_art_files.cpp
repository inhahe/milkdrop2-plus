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

#pragma comment(lib, "Normaliz.lib") // NormalizeString

namespace
{
// `text`, UTF-8, as UTF-16 in normalization form C (accented letters composed), or as it is if it
// is not valid Unicode.
std::wstring Composed(const std::string& text)
{
    std::wstring wide;
    if (const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0); n > 0)
    {
        wide.resize(static_cast<size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), n);
    }
    if (wide.empty())
        return wide;
    int size = NormalizeString(NormalizationC, wide.data(), static_cast<int>(wide.size()), nullptr, 0);
    for (int attempt = 0; attempt < 3 && size > 0; ++attempt) // the size is an estimate
    {
        std::wstring composed(static_cast<size_t>(size), L'\0');
        const int written = NormalizeString(NormalizationC, wide.data(), static_cast<int>(wide.size()), composed.data(), size);
        if (written > 0)
        {
            composed.resize(static_cast<size_t>(written));
            return composed;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            break;
        size = -written;
    }
    return wide;
}

// Whether `a` and `b` are the same name, or the same path, but for case and composition.
bool SameName(const std::string& a, const std::string& b)
{
    const std::wstring ca = Composed(a), cb = Composed(b);
    return CompareStringOrdinal(ca.data(), static_cast<int>(ca.size()), cb.data(), static_cast<int>(cb.size()), TRUE) == CSTR_EQUAL;
}

// Whether `other`, a track's album, is known to be another album than `album`, which has a title.
bool OtherAlbum(const AlbumTags& other, const AlbumTags& album)
{
    if (other.title.empty())
        return false;
    return !SameName(other.title, album.title) || (!other.artist.empty() && !album.artist.empty() && !SameName(other.artist, album.artist));
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

bool IsAlbumImage(const std::string& image, const std::string& track, const AlbumTags& album, const std::vector<std::string>& folderTracks,
                  const std::function<std::optional<AlbumTags>(const std::string&)>& albumOf)
{
    if (album.title.empty())
        return true;
    const std::string name = BaseName(image);
    if (SameName(name, BaseName(track)) || SameName(name, album.title))
        return true;
    for (const std::string& other : folderTracks)
    {
        if (SameName(other, track))
            continue;
        if (const std::optional<AlbumTags> otherAlbum = albumOf(other); otherAlbum && OtherAlbum(*otherAlbum, album))
            return false;
    }
    return true;
}
