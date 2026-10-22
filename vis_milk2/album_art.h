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

// The album art sprite's code when the image list has no [img100] section: the image in a corner,
// or the centre, with a margin of a fortieth of the canvas's shorter side; fading in, and out after
// a while if the settings say so, from the inputs above. The fade out begins `arthide` seconds
// after the art appears, but not before its fade in has ended. Faded out, the sprite stays,
// invisible, so that turning the fade off brings the art back. Not burned in: a burned sprite is
// also drawn into the preset's own image every frame, which the preset then smears.
// A sprite's x and y run from 0 to 1 across the canvas's longer side, centred on the canvas, and
// sx and sy give the image's longer side as a fraction of it; a non-square image is shrunk about
// its middle. Hence `aspect` (the canvas's width / height) and `imgaspect` (the image's). An image
// too large for the margins on one axis is centred on it. The code's own variables must not take
// the names of the sprite's inputs and outputs (r is its red, for one).
inline constexpr const char* ALBUM_ART_INIT_CODE = "burn = 0;";
inline constexpr const char* ALBUM_ART_CODE =
    "side = min(aspect, 1 / aspect);"                           // the shorter side / the longer
    "ex = 0.5 * min(aspect, 1); ey = 0.5 * min(1 / aspect, 1);" // the half width and half height shown
    "hw = 0.5 * artsize * min(imgaspect, 1);"                    // the image's half width, and half
    "hh = 0.5 * artsize * min(1 / imgaspect, 1);"                // height, / the shorter side
    "sx = artsize * side; sy = sx;"
    "x = 0.5 + artx * max(ex - side * (0.025 + hw), 0);"
    "y = 0.5 + arty * max(ey - side * (0.025 + hh), 0);"
    "fadein = if(above(artfade, 0), min(time / artfade, 1), 1);"
    "outat = max(arthide, artfade);" // when the fade out begins, if there is one
    "fadeout = if(above(arthide, 0), if(above(artfade, 0), min(max((outat + artfade - time) / artfade, 0), 1), below(time, outat)), 1);"
    "a = min(fadein, fadeout);";

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
