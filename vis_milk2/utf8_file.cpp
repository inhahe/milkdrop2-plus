/*
 * utf8_file.cpp - Reading and writing the text files the component keeps for itself.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"
#include "utf8_file.h"

#include <filesystem>
#include <fstream>
#include <iterator>

bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& text)
{
    std::wstring crlf;
    crlf.reserve(text.size() + text.size() / 16);
    for (wchar_t c : text)
    {
        if (c == L'\n')
            crlf += L"\r\n";
        else if (c != L'\r')
            crlf += c;
    }

    std::string bytes;
    if (!crlf.empty())
    {
        const int len = static_cast<int>(crlf.size());
        const int n = WideCharToMultiByte(CP_UTF8, 0, crlf.data(), len, nullptr, 0, nullptr, nullptr);
        if (n <= 0)
            return false;
        bytes.resize(static_cast<size_t>(n));
        WideCharToMultiByte(CP_UTF8, 0, crlf.data(), len, bytes.data(), n, nullptr, nullptr);
    }

    std::ofstream ofs(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!ofs)
        return false;
    ofs.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    ofs.close();
    return !ofs.fail();
}

bool ReadTextFile(const std::wstring& path, std::wstring& text)
{
    std::ifstream ifs(std::filesystem::path(path), std::ios::binary);
    if (!ifs)
        return false;
    const std::string bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad())
        return false;

    const size_t start = bytes.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    const char* p = bytes.data() + start;
    const int len = static_cast<int>(bytes.size() - start);
    std::wstring wide;
    if (len > 0)
    {
        const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, len, nullptr, 0);
        if (n > 0)
        {
            wide.resize(static_cast<size_t>(n));
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, len, wide.data(), n);
        }
        else // not UTF-8: Latin-1 from an earlier version
        {
            wide.reserve(static_cast<size_t>(len));
            for (int i = 0; i < len; ++i)
                wide += static_cast<wchar_t>(static_cast<unsigned char>(p[i]));
        }
    }

    text.clear();
    text.reserve(wide.size());
    for (size_t i = 0; i < wide.size(); ++i)
    {
        if (wide[i] == L'\r' && i + 1 < wide.size() && wide[i + 1] == L'\n')
            continue;
        text += wide[i];
    }
    return true;
}
