/*
 * album_art_files.h - Which image files the album art may come from.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

// Whether the album art may be an image file that foobar2000's album art search finds (a
// folder.jpg beside the track, or one in a covers folder), and which. Saved: do not renumber.
enum AlbumArtFiles
{
    ALBUM_ART_FILES_NEVER = 0,
    ALBUM_ART_FILES_ALBUM = 1, // only one that is the track's album's (IsAlbumImage)
    ALBUM_ART_FILES_ALWAYS = 2,
};

// An album as a track's tags name it: its title, and its album artist from the ALBUM ARTIST tag
// alone (the artists of a compilation's tracks differ). Either may be empty.
struct AlbumTags
{
    std::string title;
    std::string artist;
};

// Whether the image file at `image`, which foobar2000's album art search found for the track at
// `track`, is the cover of the track's album, `album`, rather than of another album whose tracks
// share its folder. It is unless a track in its folder is known to be of another album: its title
// differs, or its album artist does where both have one. Tracks with no album title, or whose album
// cannot be read, do not count, so a folder that holds no tracks (a covers folder, the parent of an
// album's disc folders) never makes it another's. Nor does anything when the track has no album
// title, which leaves nothing to compare, or when the image is named after the track or its album.
// `folderTracks`: the audio files in the image's folder, the track's own among them or not.
// `albumOf`: the album of the audio file at a path, or nothing if it cannot be read. Called only
//            until an album differs.
// Paths are as foobar2000 gives them ("file://C:\Music\a.flac") or plain, UTF-8. Names compare
// without regard to case or to how accented letters are composed.
bool IsAlbumImage(const std::string& image, const std::string& track, const AlbumTags& album, const std::vector<std::string>& folderTracks,
                  const std::function<std::optional<AlbumTags>(const std::string&)>& albumOf);
