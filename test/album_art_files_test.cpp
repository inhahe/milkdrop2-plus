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
    const AlbumTags gladiator = {"Gladiator", ""};

    // Whether IsAlbumImage takes `image` as the cover of `track`, whose album is `album`, with
    // `albums` the tracks in the image's folder and their albums (nothing: unreadable). `asked`
    // gets the tracks whose album was read.
    bool Check(const std::string& image, const AlbumTags& album, const std::map<std::string, std::optional<AlbumTags>>& albums,
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
        Assert::IsTrue(Check(dir + "folder.jpg", gladiator,
                             {{track, gladiator}, {dir + "02 Track.flac", gladiator}, {dir + "03 Track.mp3", AlbumTags{"gladiator", "Hans Zimmer"}}}));
    }

    TEST_METHOD(AnotherAlbumInTheFolderMakesItNot)
    {
        Assert::IsFalse(Check(dir + "folder.jpg", gladiator, {{track, gladiator}, {dir + "02 Track.flac", AlbumTags{"Labyrinth", ""}}}));
    }

    TEST_METHOD(AlbumsOfOneTitleAreToldApartByAlbumArtist)
    {
        const AlbumTags queen = {"Greatest Hits", "Queen"};
        Assert::IsFalse(Check(dir + "folder.jpg", queen, {{track, queen}, {dir + "02 Track.flac", AlbumTags{"Greatest Hits", "ABBA"}}}));
        // A compilation's tracks without an album artist are still one album.
        Assert::IsTrue(Check(dir + "folder.jpg", queen, {{track, queen}, {dir + "02 Track.flac", AlbumTags{"Greatest Hits", ""}}}));
    }

    TEST_METHOD(TracksWithNoAlbumOrUnreadableDoNotCount)
    {
        // A cue sheet's untagged WAV beside it, say.
        Assert::IsTrue(Check(dir + "folder.jpg", gladiator,
                             {{track, gladiator}, {dir + "Album.wav", AlbumTags{}}, {dir + "02 Track.flac", std::nullopt}}));
    }

    TEST_METHOD(ATrackWithNoAlbumLeavesTheSearchsChoice)
    {
        std::vector<std::string> asked;
        Assert::IsTrue(Check(dir + "folder.jpg", AlbumTags{}, {{track, AlbumTags{}}, {dir + "02 Track.flac", AlbumTags{"Labyrinth", ""}}}, &asked));
        Assert::IsTrue(asked.empty());
    }

    TEST_METHOD(AFolderWithNoTracksIsTrusted)
    {
        // A covers folder, or the parent of an album's disc folders.
        Assert::IsTrue(Check("file://D:\\Covers\\front.jpg", gladiator, {}));
    }

    TEST_METHOD(TheTrackItselfIsNotReadAndPathsIgnoreCase)
    {
        std::vector<std::string> asked;
        const std::string sameTrack = "FILE://d:\\music\\MIXED\\01 track.FLAC";
        Assert::IsTrue(Check(dir + "folder.jpg", gladiator, {{sameTrack, AlbumTags{"Labyrinth", ""}}, {dir + "02 Track.flac", gladiator}}, &asked));
        Assert::AreEqual(size_t(1), asked.size());
        Assert::AreEqual(dir + "02 Track.flac", asked[0]);
    }

    TEST_METHOD(AnImageNamedAfterTheTrackOrTheAlbumIsItsInAnyFolder)
    {
        const std::map<std::string, std::optional<AlbumTags>> mixed = {{track, gladiator}, {dir + "02 Track.flac", AlbumTags{"Labyrinth", ""}}};
        Assert::IsTrue(Check(dir + "01 Track.jpg", gladiator, mixed));
        Assert::IsTrue(Check(dir + "gladiator.PNG", gladiator, mixed));
        Assert::IsTrue(Check("D:/Music/Mixed/01 track.jpeg", gladiator, mixed));
        Assert::IsFalse(Check(dir + "Gladiator Live.jpg", gladiator, mixed));
    }

    TEST_METHOD(NamesCompareWithoutCaseOrComposition)
    {
        // "ete" with an acute accent on each e: composed (U+00E9), then as e and U+0301.
        const AlbumTags ete = {"\xC3\xA9t\xC3\xA9", ""};
        const AlbumTags eteDecomposed = {"e\xCC\x81t\x65\xCC\x81", ""};
        const std::map<std::string, std::optional<AlbumTags>> mixed = {{track, ete}, {dir + "02 Track.flac", AlbumTags{"Labyrinth", ""}}};
        Assert::IsTrue(Check(dir + "\xC3\x89T\xC3\x89.jpg", ete, mixed));      // capitals
        Assert::IsTrue(Check(dir + "E\xCC\x81T\x45\xCC\x81.jpg", ete, mixed)); // capitals, decomposed, as macOS names files
        Assert::IsTrue(Check(dir + "folder.jpg", ete, {{track, ete}, {dir + "02 Track.flac", eteDecomposed}}));
    }

    TEST_METHOD(AlbumsAreReadOnlyUntilOneDiffers)
    {
        std::vector<std::string> asked;
        Assert::IsFalse(Check(dir + "folder.jpg", gladiator,
                              {{dir + "02 Track.flac", AlbumTags{"Labyrinth", ""}}, {dir + "03 Track.flac", gladiator}, {dir + "04 Track.flac", gladiator}},
                              &asked));
        Assert::AreEqual(size_t(1), asked.size());
    }
};
} // namespace MilkDrop2
