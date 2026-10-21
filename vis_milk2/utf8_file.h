/*
 * utf8_file.h - Reading and writing the text files the component keeps for itself.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

// browser_config.ini, favorites.ini and preset_cache.dat are UTF-8 without a byte order mark.
// Earlier versions wrote them through std::wofstream in the "C" locale, which writes Latin-1 and
// stops at the first character outside it (silently dropping the rest of the file), so reading
// falls back to Latin-1 for bytes that are not valid UTF-8.

// Replaces the file at `path` with `text` encoded as UTF-8, each "\n" written as CRLF.
// Returns false if the file could not be written.
bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& text);

// Reads the file at `path` into `text`: as UTF-8 (skipping a byte order mark), or as Latin-1 if it
// is not valid UTF-8, with CRLF turned into "\n". Returns false if the file could not be read.
bool ReadTextFile(const std::wstring& path, std::wstring& text);
