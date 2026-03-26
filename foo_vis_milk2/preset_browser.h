/*
 * preset_browser.h - Category-aware preset browser with search and filtering.
 *
 * Copyright (c) 2025 Richard Albert Nichols III (Inhahe)
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// Represents a single preset with its category and full path.
struct BrowsablePreset
{
    std::wstring fullPath;      // absolute path to the .milk file
    std::wstring relativePath;  // path relative to the preset root directory
    std::wstring filename;      // just the filename (e.g. "AuthorName - Preset.milk")
    std::wstring category;      // first-level subdirectory, or L"" if in root
    float rating = 0.0f;        // cached rating from the preset file
};

// Shuffle/filter mode for preset navigation.
enum class ShuffleMode
{
    All,              // shuffle through all presets
    Category,         // shuffle within a specific category
    Favorites,        // shuffle through all favorites
    FavoritesCategory // shuffle through favorites in a specific category
};

class PresetBrowser
{
  public:
    PresetBrowser() = default;
    ~PresetBrowser() = default;

    // Scan a root directory recursively for .milk presets.
    // Populates the internal preset list and category list.
    // This is thread-safe and can be called from a background thread.
    void ScanDirectory(const std::wstring& rootDir);

    // Get all discovered categories (first-level subdirectory names).
    std::vector<std::wstring> GetCategories() const;

    // Get all presets.
    std::vector<BrowsablePreset> GetAllPresets() const;

    // Get presets in a specific category.
    std::vector<BrowsablePreset> GetPresetsByCategory(const std::wstring& category) const;

    // Search presets by filename substring (case-insensitive).
    // If category is non-empty, restricts search to that category.
    std::vector<BrowsablePreset> Search(const std::wstring& mask, const std::wstring& category = L"") const;

    // Get the total number of presets discovered.
    size_t GetPresetCount() const;

    // Get the number of categories.
    size_t GetCategoryCount() const;

    // Pick a random preset from all presets.
    BrowsablePreset GetRandomPreset() const;

    // Get presets matching any of the given categories.
    std::vector<BrowsablePreset> GetPresetsByCategories(const std::set<std::wstring>& categories) const;

    // Pick a random preset from a specific category.
    BrowsablePreset GetRandomPresetInCategory(const std::wstring& category) const;

    // Pick a random preset from any of the given categories.
    BrowsablePreset GetRandomPresetInCategories(const std::set<std::wstring>& categories) const;

    // Pick a random preset from a filtered list (e.g. favorites).
    static BrowsablePreset GetRandomFromList(const std::vector<BrowsablePreset>& presets);

    // Check if a scan has been completed.
    bool IsReady() const { return m_ready; }

    // Get the root directory that was scanned.
    std::wstring GetRootDir() const;

    // Find a BrowsablePreset by its relative path. Returns nullptr if not found.
    const BrowsablePreset* FindByRelativePath(const std::wstring& relativePath) const;

    // Get the relative path for a full path (strips the root dir prefix).
    std::wstring MakeRelativePath(const std::wstring& fullPath) const;

  private:
    void ScanDirectoryRecursive(const std::filesystem::path& dir, const std::wstring& categoryPrefix);

    std::wstring m_rootDir;
    std::vector<BrowsablePreset> m_presets;
    std::vector<std::wstring> m_categories;
    mutable std::mutex m_mutex;
    volatile bool m_ready = false;
};
