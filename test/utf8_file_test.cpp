/*
 * utf8_file_test.cpp - Tests for reading and writing the component's own text files.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vis_milk2/utf8_file.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(Utf8FileTest)
{
  private:
    std::filesystem::path dir; // this test method's own folder, so concurrent runs do not collide

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

    // Opens file `name` as a virus scanner does: others may read and write it, but not delete or
    // replace it, until the handle returned is closed.
    HANDLE HoldOpen(const wchar_t* name) const
    {
        const HANDLE h = CreateFileW(PathOf(name).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        Assert::IsTrue(h != INVALID_HANDLE_VALUE);
        return h;
    }

  public:
    TEST_METHOD_INITIALIZE(Setup)
    {
        static std::atomic<unsigned> counter;
        dir = std::filesystem::temp_directory_path() /
              (L"milkdrop2_utf8_file_test_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++counter));
        std::filesystem::create_directories(dir);
    }
    TEST_METHOD_CLEANUP(Cleanup) { std::filesystem::remove_all(dir); }

    TEST_METHOD(CharactersBeyondLatin1RoundTrip)
    {
        // Characters outside Latin-1, which paths and preset names can hold, must survive.
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
        Assert::AreEqual(std::wstring(L"unchanged"), text);
        Assert::IsTrue(ReadKeyValueFile(PathOf(L"missing.ini")).empty());
    }

    TEST_METHOD(OneBadLineDoesNotGarbleTheOthers)
    {
        // A UTF-8 line, a Latin-1 line, and a multibyte sequence cut off at the end of the file.
        WriteBytes(L"e.ini", "a=\xD0\x96\r\nb=\xE9\r\nc=\xD0");
        std::wstring text;
        Assert::IsTrue(ReadTextFile(PathOf(L"e.ini"), text));
        Assert::AreEqual(std::wstring(L"a=\u0416\nb=\u00e9\nc=\u00d0"), text);
    }

    TEST_METHOD(WritingReplacesTheFileAndLeavesNoTemporary)
    {
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"f.ini"), L"old\n"));
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"f.ini"), L"new\n"));
        Assert::AreEqual(std::string("new\r\n"), ReadBytes(L"f.ini"));
        Assert::IsFalse(std::filesystem::exists(dir / L"f.ini.tmp"));
    }

    TEST_METHOD(ReplacingWaitsForAFileBrieflyHeldOpen)
    {
        // A virus scanner or the search indexer may hold a file it has just seen written, without
        // letting it be replaced, for a moment: the write must wait for it, not give up.
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"h.ini"), L"old\n"));
        const HANDLE held = HoldOpen(L"h.ini");
        std::thread release([held] {
            Sleep(100);
            CloseHandle(held);
        });
        const bool written = WriteUtf8TextFile(PathOf(L"h.ini"), L"new\n");
        release.join();
        Assert::IsTrue(written);
        Assert::AreEqual(std::string("new\r\n"), ReadBytes(L"h.ini"));
        Assert::IsFalse(std::filesystem::exists(dir / L"h.ini.tmp"));
    }

    TEST_METHOD(AFileHeldOpenThroughoutIsLeftAsItWas)
    {
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"i.ini"), L"old\n"));
        const HANDLE held = HoldOpen(L"i.ini");
        const bool written = WriteUtf8TextFile(PathOf(L"i.ini"), L"new\n");
        CloseHandle(held);
        Assert::IsFalse(written);
        Assert::AreEqual(std::string("old\r\n"), ReadBytes(L"i.ini"));
        Assert::IsFalse(std::filesystem::exists(dir / L"i.ini.tmp"));
    }

    TEST_METHOD(AReadOnlyFileIsNotWaitedFor)
    {
        // Denied for good: waiting a second for it at each save would only stall foobar2000.
        Assert::IsTrue(WriteUtf8TextFile(PathOf(L"j.ini"), L"old\n"));
        Assert::IsTrue(SetFileAttributesW(PathOf(L"j.ini").c_str(), FILE_ATTRIBUTE_READONLY) != FALSE);
        const ULONGLONG start = GetTickCount64();
        const bool written = WriteUtf8TextFile(PathOf(L"j.ini"), L"new\n");
        const ULONGLONG elapsed = GetTickCount64() - start;
        SetFileAttributesW(PathOf(L"j.ini").c_str(), FILE_ATTRIBUTE_NORMAL); // so that Cleanup can delete it
        Assert::IsFalse(written);
        Assert::IsTrue(elapsed < 300, L"gave up only after waiting");
        Assert::AreEqual(std::string("old\r\n"), ReadBytes(L"j.ini"));
        Assert::IsFalse(std::filesystem::exists(dir / L"j.ini.tmp"));
    }

    TEST_METHOD(KeyValueLines)
    {
        WriteBytes(L"g.ini", "[Section]\r\n; comment\r\nno equals sign\r\nkey=value  \t\r\npath=C:\\a=b\\\r\nempty=\r\n");
        const auto values = ReadKeyValueFile(PathOf(L"g.ini"));
        Assert::AreEqual(size_t(3), values.size());
        Assert::AreEqual(std::wstring(L"value"), values.at(L"key"));
        Assert::AreEqual(std::wstring(L"C:\\a=b\\"), values.at(L"path"));
        Assert::AreEqual(std::wstring(), values.at(L"empty"));
    }
};
} // namespace MilkDrop2
