/*
 * album_art_files_test.cpp - Tests for which image files the album art may come from.
 *
 * Copyright (c) 2026 Inhahe
 * SPDX-License-Identifier: MIT
 */

#include "pch.h"

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <foo_vis_milk2/album_art_files.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MilkDrop2
{
TEST_CLASS(AlbumArtFilesTest)
{
  private:
    const std::string dir = "file://D:\\Music\\Mixed\\";
    const std::string track = dir + "01 Track.flac";

    // Whether IsAlbumImage takes `image` as the cover of `track`, whose album is `album`, with
    // `albums` the tracks in the image's folder and their albums (nothing: unreadable). `asked`
    // gets the tracks whose album was read.
    bool Check(const std::string& image, const std::string& album, const std::map<std::string, std::optional<std::string>>& albums,
               std::vector<std::string>* asked = nullptr) const
    {
        std::vector<std::string> paths;
        for (const auto& [path, unused] : albums)
            paths.push_back(path);
        return IsAlbumImage(image, track, album, paths, [&](const std::string& path) {
            if (asked)
                asked->push_back(path);
            return albums.at(path);
        });
    }

  public:
    TEST_METHOD(AFolderOfOneAlbumIsItsCover)
    {
        Assert::IsTrue(Check(dir + "folder.jpg", "Gladiator", {{track, "Gladiator"}, {dir + "02 Track.flac", "Gladiator"}, {dir + "03 Track.mp3", "gladiator"}}));
    }

    TEST_METHOD(AnotherAlbumInTheFolderMakesItNot)
    {
        Assert::IsFalse(Check(dir + "folder.jpg", "Gladiator", {{track, "Gladiator"}, {dir + "02 Track.flac", "Labyrinth"}}));
    }

    TEST_METHOD(ATrackWithNoAlbumInTheFolderMakesItNot)
    {
        Assert::IsFalse(Check(dir + "cover.png", "Gladiator", {{track, "Gladiator"}, {dir + "02 Track.flac", ""}}));
    }

    TEST_METHOD(UnreadableTracksDoNotCount)
    {
        Assert::IsTrue(Check(dir + "folder.jpg", "Gladiator", {{track, "Gladiator"}, {dir + "02 Track.flac", std::nullopt}}));
    }

    TEST_METHOD(AFolderWithNoTracksIsTrusted)
    {
        // A covers folder, or the parent of an album's disc folders.
        Assert::IsTrue(Check("file://D:\\Covers\\front.jpg", "Gladiator", {}));
    }

    TEST_METHOD(TheTrackItselfIsNotReadAndPathsIgnoreCase)
    {
        std::vector<std::string> asked;
        const std::string sameTrack = "FILE://d:\\music\\MIXED\\01 track.FLAC";
        Assert::IsTrue(Check(dir + "folder.jpg", "Gladiator", {{sameTrack, "Labyrinth"}, {dir + "02 Track.flac", "Gladiator"}}, &asked));
        Assert::AreEqual(size_t(1), asked.size());
        Assert::AreEqual(dir + "02 Track.flac", asked[0]);
    }

    TEST_METHOD(ATrackWithNoAlbumOwnsOnlyAFolderOfItsOwn)
    {
        Assert::IsTrue(Check(dir + "folder.jpg", "", {{track, ""}}));
        Assert::IsFalse(Check(dir + "folder.jpg", "", {{track, ""}, {dir + "02 Track.flac", ""}}));
    }

    TEST_METHOD(AnImageNamedAfterTheTrackOrTheAlbumIsItsInAnyFolder)
    {
        const std::map<std::string, std::optional<std::string>> mixed = {{track, "Gladiator"}, {dir + "02 Track.flac", "Labyrinth"}};
        Assert::IsTrue(Check(dir + "01 Track.jpg", "Gladiator", mixed));
        Assert::IsTrue(Check(dir + "gladiator.PNG", "Gladiator", mixed));
        Assert::IsTrue(Check("D:/Music/Mixed/01 track.jpeg", "", {{track, ""}, {dir + "02 Track.flac", ""}}));
        Assert::IsFalse(Check(dir + "Gladiator Live.jpg", "Gladiator", mixed));
    }

    TEST_METHOD(NamesBeyondAsciiCompareWithoutCase)
    {
        Assert::IsTrue(Check(dir + "\xC3\x89T\xC3\x89.jpg", "\xC3\xA9t\xC3\xA9", {{track, "\xC3\xA9t\xC3\xA9"}, {dir + "02 Track.flac", "Labyrinth"}}));
        Assert::IsTrue(Check(dir + "folder.jpg", "\xC3\xA9t\xC3\xA9", {{track, "x"}, {dir + "02 Track.flac", "\xC3\x89T\xC3\x89"}}));
    }

    TEST_METHOD(AlbumsAreReadOnlyUntilOneDiffers)
    {
        std::vector<std::string> asked;
        Assert::IsFalse(Check(dir + "folder.jpg", "Gladiator",
                              {{dir + "02 Track.flac", "Labyrinth"}, {dir + "03 Track.flac", "Gladiator"}, {dir + "04 Track.flac", "Gladiator"}}, &asked));
        Assert::AreEqual(size_t(1), asked.size());
    }
};
} // namespace MilkDrop2
