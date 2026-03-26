/*
 * preset_browser.cpp - Implements category-aware preset browser.
 *
 * Copyright (c) 2025 Richard Albert Nichols III (Inhahe)
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"
#include "preset_browser.h"

#include <algorithm>
#include <random>
#include <set>

void PresetBrowser::ScanDirectory(const std::wstring& rootDir)
{
    std::vector<BrowsablePreset> newPresets;
    std::set<std::wstring> newCategories;

    // Normalize root directory path (ensure trailing backslash).
    std::wstring root = rootDir;
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/')
        root += L'\\';

    if (!std::filesystem::exists(root))
        return;

    // Recursively scan.
    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied))
        {
            if (!entry.is_regular_file())
                continue;

            const auto& path = entry.path();
            if (_wcsicmp(path.extension().c_str(), L".milk") != 0)
                continue;

            BrowsablePreset preset;
            preset.fullPath = path.wstring();
            preset.filename = path.filename().wstring();

            // Compute relative path from root.
            std::wstring rel = preset.fullPath.substr(root.length());
            // Normalize separators.
            std::replace(rel.begin(), rel.end(), L'/', L'\\');
            preset.relativePath = rel;

            // Extract category from first path component.
            size_t sep = rel.find(L'\\');
            if (sep != std::wstring::npos && sep > 0)
            {
                preset.category = rel.substr(0, sep);
                newCategories.insert(preset.category);
            }

            newPresets.push_back(std::move(preset));
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // Silently handle permission errors, etc.
    }

    // Sort presets by relative path for consistent ordering.
    std::sort(newPresets.begin(), newPresets.end(), [](const BrowsablePreset& a, const BrowsablePreset& b) {
        return _wcsicmp(a.relativePath.c_str(), b.relativePath.c_str()) < 0;
    });

    // Commit results.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rootDir = root;
        m_presets = std::move(newPresets);
        m_categories.assign(newCategories.begin(), newCategories.end());
        m_ready = true;
    }
}

std::vector<std::wstring> PresetBrowser::GetCategories() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_categories;
}

std::vector<BrowsablePreset> PresetBrowser::GetAllPresets() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_presets;
}

std::vector<BrowsablePreset> PresetBrowser::GetPresetsByCategory(const std::wstring& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BrowsablePreset> result;
    for (const auto& p : m_presets)
    {
        if (_wcsicmp(p.category.c_str(), category.c_str()) == 0)
            result.push_back(p);
    }
    return result;
}

std::vector<BrowsablePreset> PresetBrowser::Search(const std::wstring& mask, const std::wstring& category) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BrowsablePreset> result;

    // Convert mask to lowercase for case-insensitive comparison.
    std::wstring lowerMask = mask;
    std::transform(lowerMask.begin(), lowerMask.end(), lowerMask.begin(), ::towlower);

    for (const auto& p : m_presets)
    {
        // Filter by category if specified.
        if (!category.empty() && _wcsicmp(p.category.c_str(), category.c_str()) != 0)
            continue;

        // Match filename against mask (case-insensitive substring search).
        std::wstring lowerFilename = p.filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::towlower);

        if (lowerFilename.find(lowerMask) != std::wstring::npos)
            result.push_back(p);
    }
    return result;
}

std::vector<BrowsablePreset> PresetBrowser::GetPresetsByCategories(const std::set<std::wstring>& categories) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BrowsablePreset> result;
    for (const auto& p : m_presets)
    {
        for (const auto& cat : categories)
        {
            if (_wcsicmp(p.category.c_str(), cat.c_str()) == 0)
            {
                result.push_back(p);
                break;
            }
        }
    }
    return result;
}

BrowsablePreset PresetBrowser::GetRandomPresetInCategories(const std::set<std::wstring>& categories) const
{
    auto presets = GetPresetsByCategories(categories);
    if (presets.empty())
        return {};
    return presets[rand() % presets.size()];
}

size_t PresetBrowser::GetPresetCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_presets.size();
}

size_t PresetBrowser::GetCategoryCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_categories.size();
}

BrowsablePreset PresetBrowser::GetRandomPreset() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_presets.empty())
        return {};
    return m_presets[rand() % m_presets.size()];
}

BrowsablePreset PresetBrowser::GetRandomPresetInCategory(const std::wstring& category) const
{
    auto presets = GetPresetsByCategory(category);
    if (presets.empty())
        return {};
    return presets[rand() % presets.size()];
}

BrowsablePreset PresetBrowser::GetRandomFromList(const std::vector<BrowsablePreset>& presets)
{
    if (presets.empty())
        return {};
    return presets[rand() % presets.size()];
}

std::wstring PresetBrowser::GetRootDir() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rootDir;
}

const BrowsablePreset* PresetBrowser::FindByRelativePath(const std::wstring& relativePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_presets)
    {
        if (_wcsicmp(p.relativePath.c_str(), relativePath.c_str()) == 0)
            return &p;
    }
    return nullptr;
}

std::wstring PresetBrowser::MakeRelativePath(const std::wstring& fullPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_rootDir.empty())
        return fullPath;

    // Check if fullPath starts with rootDir.
    if (_wcsnicmp(fullPath.c_str(), m_rootDir.c_str(), m_rootDir.length()) == 0)
        return fullPath.substr(m_rootDir.length());

    return fullPath;
}

void PresetBrowser::ScanDirectoryRecursive(const std::filesystem::path& /*dir*/, const std::wstring& /*categoryPrefix*/)
{
    // Not used - ScanDirectory uses std::filesystem::recursive_directory_iterator directly.
}
