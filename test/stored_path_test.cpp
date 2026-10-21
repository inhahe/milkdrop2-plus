/*
 * stored_path_test.cpp - Tests for how the component saves directory paths.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <string>
#include <foo_vis_milk2/stored_path.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(StoredPathTest)
{
  private:
    const std::wstring profile = L"D:\\foobar2000\\profile\\";

  public:
    TEST_METHOD(InsideProfileIsSavedRelative)
    {
        Assert::AreEqual(std::wstring(L"milkdrop2\\presets\\"), ToStoredPath(L"D:\\foobar2000\\profile\\milkdrop2\\presets\\", profile, false));
        Assert::AreEqual(std::wstring(L"milkdrop2\\presets\\"), ToStoredPath(L"D:\\foobar2000\\profile\\milkdrop2\\presets\\", profile, true));
    }

    TEST_METHOD(InsidePortableInstallationIsSavedRelative)
    {
        Assert::AreEqual(std::wstring(L"..\\original milkdrop presets\\Milkdrop-Original\\"),
                         ToStoredPath(L"D:\\foobar2000\\original milkdrop presets\\Milkdrop-Original\\", profile, true));
    }

    TEST_METHOD(BesideAnInstalledProfileStaysAbsolute)
    {
        // Without portable mode, the profile's parent is not foobar2000's folder.
        const std::wstring path = L"D:\\foobar2000\\original milkdrop presets\\";
        Assert::AreEqual(path, ToStoredPath(path, profile, false));
    }

    TEST_METHOD(ElsewhereStaysAbsolute)
    {
        const std::wstring path = L"E:\\presets\\";
        Assert::AreEqual(path, ToStoredPath(path, profile, true));
        Assert::AreEqual(path, ToStoredPath(path, profile, false));
    }

    TEST_METHOD(FoldersAreComparedWholeAndWithoutCase)
    {
        // "profile2" begins with "profile" but is not inside it.
        Assert::AreEqual(std::wstring(L"..\\profile2\\presets\\"), ToStoredPath(L"D:\\foobar2000\\profile2\\presets\\", profile, true));
        Assert::AreEqual(std::wstring(L"D:\\foobar2000\\profile2\\presets\\"), ToStoredPath(L"D:\\foobar2000\\profile2\\presets\\", profile, false));
        Assert::AreEqual(std::wstring(L"MilkDrop2\\"), ToStoredPath(L"d:\\FOOBAR2000\\Profile\\MilkDrop2\\", profile, false));
    }

    TEST_METHOD(TheProfileItselfIsNotSavedEmpty)
    {
        // An empty saved value means "not set".
        Assert::AreEqual(std::wstring(L".\\"), ToStoredPath(profile, profile, false));
        Assert::AreEqual(profile, FromStoredPath(L".\\", profile));
    }

    TEST_METHOD(TrailingSeparatorIsKeptOrNot)
    {
        Assert::AreEqual(std::wstring(L"milkdrop2\\presets"), ToStoredPath(L"D:\\foobar2000\\profile\\milkdrop2\\presets", profile, false));
        Assert::AreEqual(std::wstring(L"D:\\foobar2000\\profile\\milkdrop2\\presets"), FromStoredPath(L"milkdrop2\\presets", profile));
    }

    TEST_METHOD(RelativeIsResolvedAgainstTheProfile)
    {
        Assert::AreEqual(std::wstring(L"D:\\foobar2000\\profile\\milkdrop2\\presets\\"), FromStoredPath(L"milkdrop2\\presets\\", profile));
        Assert::AreEqual(std::wstring(L"D:\\foobar2000\\original milkdrop presets\\"), FromStoredPath(L"..\\original milkdrop presets\\", profile));
        // The same saved value finds the folder after the installation moved.
        Assert::AreEqual(std::wstring(L"C:\\fb2k\\original milkdrop presets\\"), FromStoredPath(L"..\\original milkdrop presets\\", L"C:\\fb2k\\profile\\"));
    }

    TEST_METHOD(AbsoluteAndEmptyAreLoadedUnchanged)
    {
        Assert::AreEqual(std::wstring(L"C:\\foobar2000\\presets\\"), FromStoredPath(L"C:\\foobar2000\\presets\\", profile));
        Assert::AreEqual(std::wstring(), FromStoredPath(L"", profile));
        Assert::AreEqual(std::wstring(), ToStoredPath(L"", profile, true));
    }
};
} // namespace MilkDrop2
