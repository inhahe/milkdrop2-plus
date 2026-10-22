/*
 * texture_cache_test.cpp - Tests for which loaded texture the engine drops first.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <vector>
#include <vis_milk2/texture_cache.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(TextureCacheTest)
{
  private:
    static constexpr int kPresetsLoaded = 10; // so ages 9 and 10 belong to the presets in use

    static TexInfo DiskTexture(int age, int bytes)
    {
        TexInfo t{};
        t.bEvictable = true;
        t.nAge = age;
        t.nSizeInBytes = bytes;
        return t;
    }

  public:
    TEST_METHOD(TexturesOfThePresetsInUseStay)
    {
        const std::vector<TexInfo> textures = {DiskTexture(kPresetsLoaded, 1000), DiskTexture(kPresetsLoaded - 1, 1000)};
        Assert::AreEqual(-1, PickTextureToEvict(textures, kPresetsLoaded));
    }

    TEST_METHOD(TexturesNotFromDiskStay)
    {
        TexInfo noise = DiskTexture(1, 1000);
        noise.bEvictable = false;
        Assert::AreEqual(-1, PickTextureToEvict({noise}, kPresetsLoaded));
    }

    TEST_METHOD(OfEquallyOldTexturesTheLargestGoes)
    {
        const std::vector<TexInfo> textures = {DiskTexture(3, 100), DiskTexture(3, 200), DiskTexture(3, 150)};
        Assert::AreEqual(1, PickTextureToEvict(textures, kPresetsLoaded));
    }

    TEST_METHOD(OneOldTextureGoes)
    {
        Assert::AreEqual(0, PickTextureToEvict({DiskTexture(3, 100)}, kPresetsLoaded));
    }

    TEST_METHOD(OfEquallyLargeTexturesTheLeastRecentlyUsedGoes)
    {
        const std::vector<TexInfo> textures = {DiskTexture(5, 100), DiskTexture(2, 100)};
        Assert::AreEqual(1, PickTextureToEvict(textures, kPresetsLoaded));
    }

    TEST_METHOD(RecentUseHalvesATexturesWeight)
    {
        // The least recently used counts double: 100 bytes at age 2 weighs 200.
        Assert::AreEqual(0, PickTextureToEvict({DiskTexture(2, 100), DiskTexture(5, 150)}, kPresetsLoaded));
        Assert::AreEqual(1, PickTextureToEvict({DiskTexture(2, 100), DiskTexture(5, 250)}, kPresetsLoaded));
    }
};
} // namespace MilkDrop2
