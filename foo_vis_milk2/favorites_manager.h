/*
 * favorites_manager.h - Manages favorite MilkDrop presets with persistent storage.
 *
 * Copyright (c) 2025 Richard Albert Nichols III (Inhahe)
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// A single favorite entry: stores the relative path from the preset root directory.
// e.g. "Fractal/AuthorName - Preset.milk" or just "AuthorName - Preset.milk"
struct FavoriteEntry
{
    std::wstring relativePath; // path relative to preset root, using backslash separators
    std::wstring category;     // first-level subdirectory name, or L"" if root

    bool operator<(const FavoriteEntry& other) const { return relativePath < other.relativePath; }
    bool operator==(const FavoriteEntry& other) const { return relativePath == other.relativePath; }
};

class FavoritesManager
{
  public:
    FavoritesManager() = default;
    ~FavoritesManager() = default;

    // Initialize with the path to the favorites file (e.g. milkdrop2/favorites.ini)
    void Initialize(const std::wstring& favoritesFilePath);

    // Add a preset to favorites. relativePath is relative to preset root dir.
    void Add(const std::wstring& relativePath);

    // Remove a preset from favorites.
    void Remove(const std::wstring& relativePath);

    // Toggle favorite status; returns true if now a favorite.
    bool Toggle(const std::wstring& relativePath);

    // Check if a preset is a favorite.
    bool IsFavorite(const std::wstring& relativePath) const;

    // Get all favorites.
    std::vector<FavoriteEntry> GetAll() const;

    // Get favorites filtered by category.
    std::vector<FavoriteEntry> GetByCategory(const std::wstring& category) const;

    // Get favorites matching any of the given categories.
    std::vector<FavoriteEntry> GetByCategories(const std::set<std::wstring>& categories) const;

    // Get all categories that have at least one favorite.
    std::vector<std::wstring> GetFavoriteCategories() const;

    // Get the total count of favorites.
    size_t Count() const;

    // Save to disk.
    void Save() const;

    // Load from disk.
    void Load();

  private:
    static std::wstring ExtractCategory(const std::wstring& relativePath);
    static std::wstring NormalizePath(const std::wstring& path);

    std::wstring m_filePath;
    std::set<FavoriteEntry> m_favorites;
    mutable std::recursive_mutex m_mutex;
};
