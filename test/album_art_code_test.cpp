/*
 * album_art_code_test.cpp - Tests of the album art sprite's default code (ALBUM_ART_CODE), run in
 * the engine's expression evaluator: when it fades, and where it puts the art.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <string>
#include <vis_milk2/album_art.h>
#include <projectm-eval/ns-eel2-shim/ns-eel.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The evaluator asks its host to guard its memory; plugin.cpp answers for the engine, which these
// tests leave out. They run it on one thread.
void NSEEL_HOSTSTUB_EnterMutex()
{
}
void NSEEL_HOSTSTUB_LeaveMutex()
{
}

namespace MilkDrop2
{
// The album art sprite's code in a VM of its own, with its inputs set as the engine sets them
// (texmgr::SetSpriteInputs), run at chosen times.
class AlbumArtSprite
{
  public:
    // `art`: the album art settings as inputs. `aspect`: the canvas's width / height.
    // `imgaspect`: the image's.
    AlbumArtSprite(const AlbumArtInputs& art, double aspect = 16.0 / 9.0, double imgaspect = 1.0)
    {
        m_vm = NSEEL_VM_alloc();
        *NSEEL_VM_regvar(m_vm, "aspect") = aspect;
        *NSEEL_VM_regvar(m_vm, "imgaspect") = imgaspect;
        *NSEEL_VM_regvar(m_vm, "artx") = art.x;
        *NSEEL_VM_regvar(m_vm, "arty") = art.y;
        *NSEEL_VM_regvar(m_vm, "artsize") = art.size;
        *NSEEL_VM_regvar(m_vm, "arthide") = art.hideAfter;
        *NSEEL_VM_regvar(m_vm, "artfade") = art.fadeTime;
        m_time = NSEEL_VM_regvar(m_vm, "time");
        m_x = NSEEL_VM_regvar(m_vm, "x");
        m_y = NSEEL_VM_regvar(m_vm, "y");
        m_sx = NSEEL_VM_regvar(m_vm, "sx");
        m_a = NSEEL_VM_regvar(m_vm, "a");
        std::string code = ALBUM_ART_CODE;
        m_code = NSEEL_code_compile(m_vm, code.data(), 0);
        Assert::IsNotNull(m_code, L"the album art code compiles");
    }
    ~AlbumArtSprite()
    {
        NSEEL_code_free(m_code);
        NSEEL_VM_free(m_vm);
    }
    AlbumArtSprite(const AlbumArtSprite&) = delete;
    AlbumArtSprite& operator=(const AlbumArtSprite&) = delete;

    // Runs the code `seconds` after the art appeared.
    void At(double seconds)
    {
        *m_time = seconds;
        NSEEL_code_execute(m_code);
    }
    double Alpha(double seconds)
    {
        At(seconds);
        return *m_a;
    }
    double X() const { return *m_x; }
    double Y() const { return *m_y; }
    double HalfSize() const { return 0.5 * *m_sx; } // of a square image, in the sprite's units

  private:
    NSEEL_VMCTX m_vm = nullptr;
    NSEEL_CODEHANDLE m_code = nullptr;
    double *m_time, *m_x, *m_y, *m_sx, *m_a;
};

TEST_CLASS(AlbumArtCodeTest)
{
  private:
    static AlbumArtInputs Fades(bool fadeOut, double showFor, double fadeTime)
    {
        return MakeAlbumArtInputs(ALBUM_ART_LOWER_RIGHT, DEFAULT_ALBUM_ART_SIZE, fadeOut, showFor, fadeTime);
    }
    static constexpr double kTolerance = 1e-9;

  public:
    TEST_METHOD(FadesInOverTheFadeTime)
    {
        AlbumArtSprite art(Fades(false, 10.0, 1.0));
        Assert::AreEqual(0.0, art.Alpha(0.0), kTolerance);
        Assert::AreEqual(0.5, art.Alpha(0.5), kTolerance);
        Assert::AreEqual(1.0, art.Alpha(1.0), kTolerance);
        Assert::AreEqual(1.0, art.Alpha(1000.0), kTolerance); // and stays
    }

    TEST_METHOD(WithoutAFadeItAppearsAtOnce)
    {
        AlbumArtSprite art(Fades(false, 10.0, 0.0));
        Assert::AreEqual(1.0, art.Alpha(0.0), kTolerance);
        Assert::AreEqual(1.0, art.Alpha(1000.0), kTolerance);
    }

    TEST_METHOD(FadesOutAfterTheSecondsSet)
    {
        AlbumArtSprite art(Fades(true, 5.0, 1.0));
        Assert::AreEqual(1.0, art.Alpha(3.0), kTolerance);
        Assert::AreEqual(1.0, art.Alpha(5.0), kTolerance);
        Assert::AreEqual(0.5, art.Alpha(5.5), kTolerance);
        Assert::AreEqual(0.0, art.Alpha(6.0), kTolerance);
        Assert::AreEqual(0.0, art.Alpha(60.0), kTolerance); // and stays out
    }

    TEST_METHOD(WithoutAFadeItGoesAtOnce)
    {
        AlbumArtSprite art(Fades(true, 5.0, 0.0));
        Assert::AreEqual(1.0, art.Alpha(4.99), kTolerance);
        Assert::AreEqual(0.0, art.Alpha(5.0), kTolerance);
    }

    // A fade longer than the seconds before the fade out: the art still reaches full opacity.
    TEST_METHOD(TheFadeOutWaitsForTheFadeIn)
    {
        AlbumArtSprite art(Fades(true, 1.0, 2.0));
        Assert::AreEqual(0.5, art.Alpha(1.0), kTolerance);
        Assert::AreEqual(1.0, art.Alpha(2.0), kTolerance);
        Assert::AreEqual(0.5, art.Alpha(3.0), kTolerance);
        Assert::AreEqual(0.0, art.Alpha(4.0), kTolerance);
    }

    // In each corner the image lies inside the canvas, a fortieth of its shorter side from the two
    // edges of that corner; in the centre, it is centred.
    TEST_METHOD(PlacedInItsCornerWithinTheCanvas)
    {
        for (const double aspect : {16.0 / 9.0, 9.0 / 16.0})
        {
            const double side = aspect > 1 ? 1 / aspect : aspect;                    // the shorter side / the longer
            const double halfWidth = 0.5 * (aspect > 1 ? 1 : aspect);                // shown, in the sprite's units
            const double halfHeight = 0.5 * (aspect > 1 ? 1 / aspect : 1);
            const int corners[][3] = {{ALBUM_ART_LOWER_RIGHT, 1, 1}, {ALBUM_ART_LOWER_LEFT, -1, 1}, {ALBUM_ART_UPPER_RIGHT, 1, -1}, {ALBUM_ART_UPPER_LEFT, -1, -1}};
            for (const auto& [position, dx, dy] : corners)
            {
                AlbumArtSprite art(MakeAlbumArtInputs(position, DEFAULT_ALBUM_ART_SIZE, false, 10.0, 0.5), aspect);
                art.At(10.0);
                Assert::AreEqual(0.5 + dx * (halfWidth - 0.025 * side), art.X() + dx * art.HalfSize(), kTolerance, L"margin across");
                Assert::AreEqual(0.5 + dy * (halfHeight - 0.025 * side), art.Y() + dy * art.HalfSize(), kTolerance, L"margin down");
                Assert::AreEqual(DEFAULT_ALBUM_ART_SIZE / 100.0 * side, 2 * art.HalfSize(), kTolerance, L"size");
            }
            AlbumArtSprite centered(MakeAlbumArtInputs(ALBUM_ART_CENTER, DEFAULT_ALBUM_ART_SIZE, false, 10.0, 0.5), aspect);
            centered.At(10.0);
            Assert::AreEqual(0.5, centered.X(), kTolerance);
            Assert::AreEqual(0.5, centered.Y(), kTolerance);
        }
    }

    // An image too large for the margins on an axis is centred on that axis.
    TEST_METHOD(TooLargeForTheMarginsIsCentred)
    {
        AlbumArtSprite art(MakeAlbumArtInputs(ALBUM_ART_LOWER_RIGHT, MAX_ALBUM_ART_SIZE, false, 10.0, 0.5), 16.0 / 9.0);
        art.At(10.0);
        Assert::AreEqual(0.5, art.Y(), kTolerance); // as tall as the canvas: no room for a margin
        Assert::IsTrue(art.X() > 0.5);              // but still to the right
    }
};
} // namespace MilkDrop2
