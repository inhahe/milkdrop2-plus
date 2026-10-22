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

namespace
{
// One line's bytes as UTF-8, or as Latin-1 if they are not valid UTF-8.
std::wstring DecodeLine(const char* p, size_t len)
{
    std::wstring line;
    if (len == 0)
        return line;
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, static_cast<int>(len), nullptr, 0);
    if (n > 0)
    {
        line.resize(static_cast<size_t>(n));
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, static_cast<int>(len), line.data(), n);
    }
    else
    {
        line.reserve(len);
        for (size_t i = 0; i < len; ++i)
            line += static_cast<wchar_t>(static_cast<unsigned char>(p[i]));
    }
    return line;
}

// Puts the file at `from` in place of the one at `to`. A file just written is often opened for a
// moment by a virus scanner, the search indexer or a sync client, which do not let it be replaced
// meanwhile, so the move is tried again for about a second before it gives up. Returns whether it
// was moved.
bool MoveReplacing(const std::wstring& from, const std::wstring& to)
{
    for (DWORD delay = 1;; delay *= 2)
    {
        if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
        const DWORD error = GetLastError();
        if ((error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) || delay > 512)
            return false;
        Sleep(delay); // 1 + 2 + ... + 512 ms: about a second in all
    }
}
} // namespace

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

    // Write a temporary file and put it in place of the old one, so that a crash or a full disk
    // part-way through leaves the old file intact rather than a truncated one.
    const std::wstring temp = path + L".tmp";
    {
        std::ofstream ofs(std::filesystem::path(temp), std::ios::binary | std::ios::trunc);
        if (!ofs)
            return false;
        ofs.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        ofs.close();
        if (ofs.fail())
        {
            DeleteFileW(temp.c_str());
            return false;
        }
    }
    if (!MoveReplacing(temp, path))
    {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

bool ReadTextFile(const std::wstring& path, std::wstring& text)
{
    std::ifstream ifs(std::filesystem::path(path), std::ios::binary);
    if (!ifs)
        return false;
    const std::string bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad())
        return false;

    text.clear();
    text.reserve(bytes.size());
    size_t pos = bytes.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    while (pos <= bytes.size())
    {
        size_t end = bytes.find('\n', pos);
        const bool bLast = end == std::string::npos;
        if (bLast)
            end = bytes.size();
        size_t len = end - pos;
        if (len > 0 && bytes[pos + len - 1] == '\r')
            --len;
        text += DecodeLine(bytes.data() + pos, len);
        if (bLast)
            break;
        text += L'\n';
        pos = end + 1;
    }
    return true;
}

std::map<std::wstring, std::wstring> ReadKeyValueFile(const std::wstring& path)
{
    std::map<std::wstring, std::wstring> values;
    std::wstring text;
    if (!ReadTextFile(path, text))
        return values;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find(L'\n', pos);
        if (end == std::wstring::npos)
            end = text.size();
        const std::wstring line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty() || line[0] == L'[' || line[0] == L';')
            continue;
        const size_t eq = line.find(L'=');
        if (eq == std::wstring::npos)
            continue;
        std::wstring value = line.substr(eq + 1);
        while (!value.empty() && (value.back() == L' ' || value.back() == L'\t'))
            value.pop_back();
        values[line.substr(0, eq)] = value;
    }
    return values;
}
