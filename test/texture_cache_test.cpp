/*
 * texture_cache_test.cpp - Tests for the limits of the engine's texture cache and which
 * texture it drops first.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <climits>
#include <cstdint>
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
    const std::vector<const ID3D11Resource*> noneInUse;

    // A stand-in for a texture object, never dereferenced.
    static ID3D11Resource* FakeResource(uintptr_t n) { return reinterpret_cast<ID3D11Resource*>(n * 16); }

    static TexInfo DiskTexture(int age, int bytes, uintptr_t id = 1)
    {
        TexInfo t{};
        t.texptr = FakeResource(id);
        t.bEvictable = true;
        t.nAge = age;
        t.nSizeInBytes = bytes;
        return t;
    }

  public:
    TEST_METHOD(TexturesOfThePresetsInUseStay)
    {
        const std::vector<TexInfo> textures = {DiskTexture(kPresetsLoaded, 1000), DiskTexture(kPresetsLoaded - 1, 1000)};
        Assert::AreEqual(-1, PickTextureToEvict(textures, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(TexturesNotFromDiskStay)
    {
        TexInfo noise = DiskTexture(1, 1000);
        noise.bEvictable = false;
        Assert::AreEqual(-1, PickTextureToEvict({noise}, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(TexturesBoundToAShaderStay)
    {
        // The larger one is bound to a shader still drawn (the preset a blend comes from).
        const std::vector<TexInfo> textures = {DiskTexture(2, 100, 1), DiskTexture(2, 500, 2)};
        Assert::AreEqual(0, PickTextureToEvict(textures, kPresetsLoaded, {FakeResource(2)}));
        Assert::AreEqual(-1, PickTextureToEvict(textures, kPresetsLoaded, {FakeResource(1), FakeResource(2)}));
    }

    TEST_METHOD(OfEquallyOldTexturesTheLargestGoes)
    {
        const std::vector<TexInfo> textures = {DiskTexture(3, 100), DiskTexture(3, 200), DiskTexture(3, 150)};
        Assert::AreEqual(1, PickTextureToEvict(textures, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(OneOldTextureGoes)
    {
        Assert::AreEqual(0, PickTextureToEvict({DiskTexture(3, 100)}, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(OfEquallyLargeTexturesTheLeastRecentlyUsedGoes)
    {
        const std::vector<TexInfo> textures = {DiskTexture(5, 100), DiskTexture(2, 100)};
        Assert::AreEqual(1, PickTextureToEvict(textures, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(RecentUseHalvesATexturesWeight)
    {
        // The least recently used counts double: 100 bytes at age 2 weighs 200.
        Assert::AreEqual(0, PickTextureToEvict({DiskTexture(2, 100), DiskTexture(5, 150)}, kPresetsLoaded, noneInUse));
        Assert::AreEqual(1, PickTextureToEvict({DiskTexture(2, 100), DiskTexture(5, 250)}, kPresetsLoaded, noneInUse));
    }

    TEST_METHOD(CacheIsFullAtEitherLimit)
    {
        const std::vector<TexInfo> textures = {DiskTexture(2, 100), DiskTexture(3, 200)};
        Assert::IsFalse(TextureCacheIsFull(textures, 3, 301));
        Assert::IsTrue(TextureCacheIsFull(textures, 2, 301));
        Assert::IsTrue(TextureCacheIsFull(textures, 3, 300));
    }

    TEST_METHOD(OnlyDiskTexturesCount)
    {
        TexInfo noise = DiskTexture(2, 100);
        noise.bEvictable = false;
        Assert::IsFalse(TextureCacheIsFull({noise, noise}, 1, 50));
    }

    TEST_METHOD(AutoSetsNoLimit)
    {
        // "Auto" in the preferences is stored as -1.
        const std::vector<TexInfo> textures = {DiskTexture(2, 100), DiskTexture(3, 200)};
        Assert::IsFalse(TextureCacheIsFull(textures, -1, -1));
        Assert::IsTrue(TextureCacheIsFull(textures, -1, 300));
        Assert::IsTrue(TextureCacheIsFull(textures, 2, -1));
    }

    TEST_METHOD(BytesBeyondAnIntStillCount)
    {
        const std::vector<TexInfo> textures = {DiskTexture(2, INT_MAX), DiskTexture(3, INT_MAX)};
        Assert::IsTrue(TextureCacheIsFull(textures, 100, INT_MAX));
    }

    TEST_METHOD(SizeOfALargeTexture)
    {
        // 8192 x 8192 RGBA: 256 MiB, whose size in bits does not fit in an int.
        Assert::AreEqual(8192 * 8192 * 4 + 16384, TextureSizeInBytes(8192, 8192, 1, 32));
        Assert::AreEqual(INT_MAX, TextureSizeInBytes(65536, 65536, 1, 128));
        Assert::AreEqual(64 * 64 * 64 * 4 + 16384, TextureSizeInBytes(64, 64, 64, 32));
    }
};
} // namespace MilkDrop2
