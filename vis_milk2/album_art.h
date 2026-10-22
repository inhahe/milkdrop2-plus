/*
 * album_art.h - How the album art sprite is drawn when the image list does not say.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>

// The album art settings, which draw the art when the image list (milk2_img.ini) has no [img100]
// section: its position, its size in percent, and whether, after how long and how fast it fades.
// The positions' values are saved: add new ones at the end.
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
static constexpr double MIN_ALBUM_ART_SHOW_FOR = 1.0; // seconds before the art fades out, when it does
static constexpr double MAX_ALBUM_ART_SHOW_FOR = 3600.0;
static constexpr double DEFAULT_ALBUM_ART_SHOW_FOR = 10.0;
static constexpr double MAX_ALBUM_ART_FADE_TIME = 60.0; // seconds a fade in or out takes
static constexpr double DEFAULT_ALBUM_ART_FADE_TIME = 0.5;

// The album art settings as the sprites' inputs: `artx` and `arty` (x, y) are -1, 0 or 1 for
// left or top, centre, and right or bottom; `artsize` (size) is the image's longer side as a
// fraction of the canvas's shorter side; `arthide` (hideAfter) is the seconds after which it
// starts to fade out, or 0 if it does not; `artfade` (fadeTime) is the seconds a fade in or out
// takes, 0 for none.
struct AlbumArtInputs
{
    int x;
    int y;
    double size;
    double hideAfter;
    double fadeTime;
};

// `seconds` held to [lo, hi]; not a number is `lo`.
inline double ClampAlbumArtSeconds(double seconds, double lo, double hi)
{
    return seconds >= lo ? (seconds <= hi ? seconds : hi) : lo;
}

// `position`: an AlbumArtPosition; one this version does not know (a newer version's) is taken as
// the lower right corner. `percent`: the size setting. `fadeOut`: whether the art fades out after
// `showFor` seconds. `fadeTime`: the seconds a fade takes. The numbers are held to the settings'
// ranges.
inline AlbumArtInputs MakeAlbumArtInputs(int position, int percent, bool fadeOut, double showFor, double fadeTime)
{
    AlbumArtInputs inputs{1, 1, std::clamp(percent, MIN_ALBUM_ART_SIZE, MAX_ALBUM_ART_SIZE) / 100.0,
                          fadeOut ? ClampAlbumArtSeconds(showFor, MIN_ALBUM_ART_SHOW_FOR, MAX_ALBUM_ART_SHOW_FOR) : 0.0,
                          ClampAlbumArtSeconds(fadeTime, 0.0, MAX_ALBUM_ART_FADE_TIME)};
    switch (position)
    {
        case ALBUM_ART_LOWER_LEFT: inputs.x = -1; break;
        case ALBUM_ART_UPPER_RIGHT: inputs.y = -1; break;
        case ALBUM_ART_UPPER_LEFT: inputs.x = inputs.y = -1; break;
        case ALBUM_ART_CENTER: inputs.x = inputs.y = 0; break;
        default: break; // ALBUM_ART_LOWER_RIGHT
    }
    return inputs;
}
