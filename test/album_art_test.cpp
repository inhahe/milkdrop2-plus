/*
 * album_art_test.cpp - Tests for where the album art settings put the album art sprite.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <vis_milk2/album_art.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(AlbumArtTest)
{
  private:
    static void AssertCorner(int position, int x, int y)
    {
        const AlbumArtPlacement placement = PlaceAlbumArt(position, DEFAULT_ALBUM_ART_SIZE);
        Assert::AreEqual(x, placement.x);
        Assert::AreEqual(y, placement.y);
    }

  public:
    // x: -1 left, 1 right; y: -1 top, 1 bottom (a sprite's y grows downwards).
    TEST_METHOD(EachPositionGoesToItsPlace)
    {
        AssertCorner(ALBUM_ART_LOWER_RIGHT, 1, 1);
        AssertCorner(ALBUM_ART_LOWER_LEFT, -1, 1);
        AssertCorner(ALBUM_ART_UPPER_RIGHT, 1, -1);
        AssertCorner(ALBUM_ART_UPPER_LEFT, -1, -1);
        AssertCorner(ALBUM_ART_CENTER, 0, 0);
    }

    // The positions are saved as these numbers.
    TEST_METHOD(SavedPositionsKeepTheirValues)
    {
        Assert::AreEqual(0, static_cast<int>(ALBUM_ART_LOWER_RIGHT));
        Assert::AreEqual(1, static_cast<int>(ALBUM_ART_LOWER_LEFT));
        Assert::AreEqual(2, static_cast<int>(ALBUM_ART_UPPER_RIGHT));
        Assert::AreEqual(3, static_cast<int>(ALBUM_ART_UPPER_LEFT));
        Assert::AreEqual(4, static_cast<int>(ALBUM_ART_CENTER));
    }

    TEST_METHOD(AnUnknownPositionIsTheLowerRight)
    {
        AssertCorner(ALBUM_ART_CENTER + 1, 1, 1);
        AssertCorner(-1, 1, 1);
    }

    TEST_METHOD(SizeIsAFractionOfTheShorterSide)
    {
        Assert::AreEqual(0.2, PlaceAlbumArt(ALBUM_ART_LOWER_RIGHT, 20).size, 1e-12);
        Assert::AreEqual(1.0, PlaceAlbumArt(ALBUM_ART_LOWER_RIGHT, 100).size, 1e-12);
    }

    TEST_METHOD(SizeIsHeldToTheSettingsRange)
    {
        Assert::AreEqual(MIN_ALBUM_ART_SIZE / 100.0, PlaceAlbumArt(ALBUM_ART_LOWER_RIGHT, 0).size, 1e-12);
        Assert::AreEqual(MIN_ALBUM_ART_SIZE / 100.0, PlaceAlbumArt(ALBUM_ART_LOWER_RIGHT, -50).size, 1e-12);
        Assert::AreEqual(MAX_ALBUM_ART_SIZE / 100.0, PlaceAlbumArt(ALBUM_ART_LOWER_RIGHT, 250).size, 1e-12);
    }
};
} // namespace MilkDrop2
