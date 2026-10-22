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

// Whether the image file at `image`, which foobar2000's album art search found for the track at
// `track`, is the cover of the track's album, `album` (empty if the track has none), rather than of
// another album whose tracks share its folder. It is if it is named after the track or the album
// (a cover for one file or one album, wherever it is kept), or if no other track in its folder has
// another album, which includes a folder that holds no tracks, such as a covers folder or the
// parent of an album's disc folders. A track with no album cannot be told from another album's, so
// then any other track in the folder makes the image not its.
// `folderTracks`: the audio files in the image's folder, the track's own among them or not.
// `albumOf`: the album of the audio file at a path (empty if it has none), or nothing if it cannot
//            be read, which then does not count. Called only until an album differs.
// Paths are as foobar2000 gives them ("file://C:\Music\a.flac") or plain, UTF-8; names compare
// without regard to case.
bool IsAlbumImage(const std::string& image, const std::string& track, const std::string& album, const std::vector<std::string>& folderTracks,
                  const std::function<std::optional<std::string>(const std::string&)>& albumOf);
