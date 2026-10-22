/*
 * album_art.h - Where the album art sprite goes when the image list does not say.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>

// The album art settings, which place the art when the image list (milk2_img.ini) has no [img100]
// section: its position, and its size in percent. The positions' values are saved: add new ones
// at the end.
enum AlbumArtPosition : int
{
    ALBUM_ART_LOWER_RIGHT,
    ALBUM_ART_LOWER_LEFT,
    ALBUM_ART_UPPER_RIGHT,
    ALBUM_ART_UPPER_LEFT,
    ALBUM_ART_CENTER,
};
static constexpr int MIN_ALBUM_ART_SIZE = 10;
static constexpr int MAX_ALBUM_ART_SIZE = 100;
static constexpr int DEFAULT_ALBUM_ART_SIZE = 20;

// The album art sprite's inputs `artx`, `arty` and `artsize`: x and y are -1, 0 or 1 for left or
// top, centre, and right or bottom; size is the image's longer side as a fraction of the canvas's
// shorter side.
struct AlbumArtPlacement
{
    int x;
    int y;
    double size;
};

// `position`: an AlbumArtPosition; one this version does not know (a newer version's) is taken
// as the lower right corner. `percent`: the size setting, held to the settings' range.
inline AlbumArtPlacement PlaceAlbumArt(int position, int percent)
{
    AlbumArtPlacement placement{1, 1, std::clamp(percent, MIN_ALBUM_ART_SIZE, MAX_ALBUM_ART_SIZE) / 100.0};
    switch (position)
    {
        case ALBUM_ART_LOWER_LEFT: placement.x = -1; break;
        case ALBUM_ART_UPPER_RIGHT: placement.y = -1; break;
        case ALBUM_ART_UPPER_LEFT: placement.x = placement.y = -1; break;
        case ALBUM_ART_CENTER: placement.x = placement.y = 0; break;
        default: break; // ALBUM_ART_LOWER_RIGHT
    }
    return placement;
}
