/*
 * utf8_file_test.cpp - Tests for reading and writing the component's own text files.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vis_milk2/utf8_file.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(Utf8FileTest)
{
  private:
    std::filesystem::path dir = std::filesystem::temp_directory_path() / L"milkdrop2_utf8_file_test";

    std::wstring PathOf(const wchar_t* name) const { return (dir / name).wstring(); }

    void WriteBytes(const wchar_t* name, const std::string& bytes) const
    {
        std::ofstream(dir / name, std::ios::binary) << bytes;
    }

    std::string ReadBytes(const wchar_t* name) const
    {
        std::ifstream ifs(dir / name, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

  public:
    TEST_METHOD_INITIALIZE(Setup) { std::filesystem::create_directories(dir); }
    TEST_METHOD_CLEANUP(Cleanup) { std::filesystem::remove_all(dir); }

    TEST_METHOD(CharactersBeyondLatin1RoundTrip)
    {
        // The default-locale std::wofstream used before stopped writing at the first of these.
        const std::wstring text = L"[BrowserConfig]\nPresetLibraryPath=C:\\Users\\\u0418\u0432\u0430\u043d\\\u30d7\u30ea\u30bb\u30c3\u30c8\\\nCategoriesConfigured=1\n";
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"a.ini"), text));
        std::wstring back;
        Assert::IsTrue(ReadTextFile(PathOf(L"a.ini"), back));
        Assert::AreEqual(text, back);
    }

    TEST_METHOD(WritesUtf8WithCrlfAndNoByteOrderMark)
    {
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"b.ini"), L"\u00e9\n"));
        Assert::AreEqual(std::string("\xC3\xA9\r\n"), ReadBytes(L"b.ini"));
    }

    TEST_METHOD(Latin1FromEarlierVersionsIsRead)
    {
        WriteBytes(L"c.ini", "PresetLibraryPath=D:\\Pr\xE9r\xE9glages\\\r\ncount=1\r\n");
        std::wstring text;
        Assert::IsTrue(ReadTextFile(PathOf(L"c.ini"), text));
        Assert::AreEqual(std::wstring(L"PresetLibraryPath=D:\\Pr\u00e9r\u00e9glages\\\ncount=1\n"), text);
    }

    TEST_METHOD(ByteOrderMarkIsSkipped)
    {
        WriteBytes(L"d.ini", "\xEF\xBB\xBFkey=\xD0\x96\r\n");
        std::wstring text;
        Assert::IsTrue(ReadTextFile(PathOf(L"d.ini"), text));
        Assert::AreEqual(std::wstring(L"key=\u0416\n"), text);
    }

    TEST_METHOD(MissingFileIsNotRead)
    {
        std::wstring text = L"unchanged";
        Assert::IsFalse(ReadTextFile(PathOf(L"missing.ini"), text));
    }
};
} // namespace MilkDrop2
