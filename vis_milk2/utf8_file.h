/*
 * utf8_file.h - Reading and writing the text files the component keeps for itself.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <map>
#include <string>

// browser_config.ini, favorites.ini and preset_cache.dat are UTF-8 without a byte order mark.
// Earlier versions wrote them through std::wofstream in the "C" locale, which writes Latin-1 and
// stops at the first character outside it (silently dropping the rest of the file), so reading
// falls back to Latin-1, line by line, for bytes that are not valid UTF-8.

// Replaces the file at `path` with `text` encoded as UTF-8, each "\n" written as CRLF, through a
// temporary file so that a failure leaves the old file whole. Returns false if it could not be
// written (the old file is then unchanged).
bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& text);

// Reads the file at `path` into `text`, with lines separated by "\n" (CRLF or LF in the file):
// each line as UTF-8 (a leading byte order mark is skipped), or as Latin-1 if that line is not
// valid UTF-8. Returns false if the file could not be read.
bool ReadTextFile(const std::wstring& path, std::wstring& text);

// The "key=value" lines of the file at `path`, read by ReadTextFile(), each value without trailing
// spaces or tabs; lines that are empty, start with '[' or ';', or have no '=' are skipped. Empty if
// the file is missing or unreadable.
std::map<std::wstring, std::wstring> ReadKeyValueFile(const std::wstring& path);
