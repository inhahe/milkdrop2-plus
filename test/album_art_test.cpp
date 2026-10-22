/*
 * album_art_test.cpp - Tests for how the album art settings reach the album art sprite.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <limits>
#include <vis_milk2/album_art.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(AlbumArtTest)
{
  private:
    // The inputs for `position` and the default size, with no fade out.
    static AlbumArtInputs At(int position) { return MakeAlbumArtInputs(position, DEFAULT_ALBUM_ART_SIZE, false, 10.0, 0.5); }
    static AlbumArtInputs Sized(int percent) { return MakeAlbumArtInputs(ALBUM_ART_LOWER_RIGHT, percent, false, 10.0, 0.5); }
    static AlbumArtInputs Fading(bool fadeOut, double showFor, double fadeTime)
    {
        return MakeAlbumArtInputs(ALBUM_ART_LOWER_RIGHT, DEFAULT_ALBUM_ART_SIZE, fadeOut, showFor, fadeTime);
    }

    static void AssertCorner(int position, int x, int y)
    {
        const AlbumArtInputs inputs = At(position);
        Assert::AreEqual(x, inputs.x);
        Assert::AreEqual(y, inputs.y);
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
        Assert::AreEqual(0.2, Sized(20).size, 1e-12);
        Assert::AreEqual(1.0, Sized(100).size, 1e-12);
    }

    TEST_METHOD(SizeIsHeldToTheSettingsRange)
    {
        Assert::AreEqual(MIN_ALBUM_ART_SIZE / 100.0, Sized(0).size, 1e-12);
        Assert::AreEqual(MIN_ALBUM_ART_SIZE / 100.0, Sized(-50).size, 1e-12);
        Assert::AreEqual(MAX_ALBUM_ART_SIZE / 100.0, Sized(250).size, 1e-12);
    }

    // `arthide` 0 means the art stays.
    TEST_METHOD(WithoutFadeOutTheArtStays)
    {
        Assert::AreEqual(0.0, Fading(false, 10.0, 0.5).hideAfter);
        Assert::AreEqual(0.0, Fading(false, 99.0, 2.0).hideAfter);
    }

    TEST_METHOD(FadeOutStartsAfterTheSecondsSet)
    {
        Assert::AreEqual(10.0, Fading(true, 10.0, 0.5).hideAfter);
        Assert::AreEqual(2.5, Fading(true, 2.5, 0.5).hideAfter);
    }

    // At least MIN_ALBUM_ART_SHOW_FOR, so that fading out never reads as "stays" (0).
    TEST_METHOD(SecondsBeforeTheFadeAreHeldToTheRange)
    {
        Assert::AreEqual(MIN_ALBUM_ART_SHOW_FOR, Fading(true, 0.0, 0.5).hideAfter);
        Assert::AreEqual(MIN_ALBUM_ART_SHOW_FOR, Fading(true, -3.0, 0.5).hideAfter);
        Assert::AreEqual(MAX_ALBUM_ART_SHOW_FOR, Fading(true, 1e9, 0.5).hideAfter);
        Assert::AreEqual(MIN_ALBUM_ART_SHOW_FOR, Fading(true, std::numeric_limits<double>::quiet_NaN(), 0.5).hideAfter);
    }

    TEST_METHOD(FadeTimeIsHeldToTheRange)
    {
        Assert::AreEqual(0.5, Fading(false, 10.0, 0.5).fadeTime);
        Assert::AreEqual(0.0, Fading(false, 10.0, 0.0).fadeTime);
        Assert::AreEqual(0.0, Fading(false, 10.0, -1.0).fadeTime);
        Assert::AreEqual(MAX_ALBUM_ART_FADE_TIME, Fading(false, 10.0, 1e9).fadeTime);
        Assert::AreEqual(0.0, Fading(false, 10.0, std::numeric_limits<double>::quiet_NaN()).fadeTime);
    }
};
} // namespace MilkDrop2
