/*
 * favorites_manager.cpp - Implements favorite preset management with persistent storage.
 *
 * Copyright (c) 2025 Richard Albert Nichols III (Inhahe)
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"
#include "favorites_manager.h"

#include <algorithm>
#include <fstream>

void FavoritesManager::Initialize(const std::wstring& favoritesFilePath)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_filePath = favoritesFilePath;
    // Don't auto-load here; caller should call Load() explicitly.
}

void FavoritesManager::Add(const std::wstring& relativePath)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    FavoriteEntry entry;
    entry.relativePath = NormalizePath(relativePath);
    entry.category = ExtractCategory(entry.relativePath);
    m_favorites.insert(entry);
    // Auto-save after modification.
    // (lock is held, but Save() uses m_favorites directly)
}

void FavoritesManager::Remove(const std::wstring& relativePath)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    FavoriteEntry entry;
    entry.relativePath = NormalizePath(relativePath);
    m_favorites.erase(entry);
}

bool FavoritesManager::Toggle(const std::wstring& relativePath)
{
    std::wstring normalized = NormalizePath(relativePath);
    FavoriteEntry entry;
    entry.relativePath = normalized;
    entry.category = ExtractCategory(normalized);

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_favorites.find(entry);
    if (it != m_favorites.end())
    {
        m_favorites.erase(it);
        return false;
    }
    else
    {
        m_favorites.insert(entry);
        return true;
    }
}

bool FavoritesManager::IsFavorite(const std::wstring& relativePath) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    FavoriteEntry entry;
    entry.relativePath = NormalizePath(relativePath);
    return m_favorites.count(entry) > 0;
}

std::vector<FavoriteEntry> FavoritesManager::GetAll() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return std::vector<FavoriteEntry>(m_favorites.begin(), m_favorites.end());
}

std::vector<FavoriteEntry> FavoritesManager::GetByCategory(const std::wstring& category) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<FavoriteEntry> result;
    for (const auto& fav : m_favorites)
    {
        if (_wcsicmp(fav.category.c_str(), category.c_str()) == 0)
            result.push_back(fav);
    }
    return result;
}

std::vector<FavoriteEntry> FavoritesManager::GetByCategories(const std::set<std::wstring>& categories) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<FavoriteEntry> result;
    for (const auto& fav : m_favorites)
    {
        for (const auto& cat : categories)
        {
            if (_wcsicmp(fav.category.c_str(), cat.c_str()) == 0)
            {
                result.push_back(fav);
                break;
            }
        }
    }
    return result;
}

std::vector<std::wstring> FavoritesManager::GetFavoriteCategories() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::set<std::wstring> catSet;
    for (const auto& fav : m_favorites)
    {
        if (!fav.category.empty())
            catSet.insert(fav.category);
    }
    return std::vector<std::wstring>(catSet.begin(), catSet.end());
}

size_t FavoritesManager::Count() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_favorites.size();
}

void FavoritesManager::Save() const
{
    if (m_filePath.empty())
        return;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Ensure parent directory exists.
    std::filesystem::path p(m_filePath);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    std::wofstream ofs(m_filePath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open())
        return;

    ofs << L"[Favorites]" << std::endl;
    int idx = 0;
    for (const auto& fav : m_favorites)
    {
        ofs << L"preset" << idx << L"=" << fav.relativePath << std::endl;
        idx++;
    }
    ofs << L"count=" << m_favorites.size() << std::endl;
}

void FavoritesManager::Load()
{
    if (m_filePath.empty())
        return;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_favorites.clear();

    if (!std::filesystem::exists(m_filePath))
        return;

    std::wifstream ifs(m_filePath);
    if (!ifs.is_open())
        return;

    std::wstring line;
    while (std::getline(ifs, line))
    {
        // Skip section headers and empty lines.
        if (line.empty() || line[0] == L'[' || line[0] == L';')
            continue;

        // Parse "presetN=relative/path.milk" lines.
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos)
            continue;

        std::wstring key = line.substr(0, eq);
        std::wstring value = line.substr(eq + 1);

        // Skip the "count=" line.
        if (key == L"count")
            continue;

        // Accept any "preset*=" line.
        if (key.substr(0, 6) == L"preset" && !value.empty())
        {
            FavoriteEntry entry;
            entry.relativePath = NormalizePath(value);
            entry.category = ExtractCategory(entry.relativePath);
            m_favorites.insert(entry);
        }
    }
}

std::wstring FavoritesManager::ExtractCategory(const std::wstring& relativePath)
{
    // Category is the first path component if it contains a separator.
    size_t sep = relativePath.find_first_of(L"\\/");
    if (sep != std::wstring::npos && sep > 0)
        return relativePath.substr(0, sep);
    return L"";
}

std::wstring FavoritesManager::NormalizePath(const std::wstring& path)
{
    // Normalize to backslash, trim leading/trailing whitespace and separators.
    std::wstring result = path;
    // Replace forward slashes with backslashes.
    std::replace(result.begin(), result.end(), L'/', L'\\');
    // Trim leading backslash.
    while (!result.empty() && result.front() == L'\\')
        result.erase(result.begin());
    // Trim trailing whitespace.
    while (!result.empty() && (result.back() == L' ' || result.back() == L'\t' || result.back() == L'\r' || result.back() == L'\n'))
        result.pop_back();
    return result;
}
