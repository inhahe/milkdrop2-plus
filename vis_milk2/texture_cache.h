/*
  LICENSE
  -------
  Copyright 2005-2013 Nullsoft, Inc.
  Copyright 2021-2024 Jimmy Cassis
  Copyright 2026 Inhahe
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of Nullsoft nor the names of its contributors may be used to
      endorse or promote products derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// texture_cache.h - The textures MilkDrop keeps loaded, and which of them to drop first.

#pragma once

#include <vector>

struct ID3D11Resource;

// A texture the engine keeps loaded (`CPlugin::m_textures`): one loaded from disk for a preset's
// shaders, or one made at startup (noise textures and the like).
struct TexInfo
{
    wchar_t texname[256]; // ~filename, but without path or extension!
    ID3D11Resource* texptr;
    int w, h, d;
    bool bEvictable;  // loaded from disk, so it may be dropped to keep within the cache limits
    int nAge;         // the `CPlugin::m_nPresetsLoadedTotal` of its last use; only valid if bEvictable is true
    int nSizeInBytes; // only valid if bEvictable is true
};

// The size a texture of `w` x `h` x `d` texels of `bitsPerPixel` takes, for the cache limits,
// clamped to what an int holds.
int TextureSizeInBytes(unsigned w, unsigned h, unsigned d, int bitsPerPixel);

// Whether the textures loaded from disk reach the cache limits, so that one must go before
// another loads. `maxImages`, `maxBytes`: the MaxImages and MaxBytes settings; a negative one
// (the preferences' "Auto") sets no limit of its own, leaving only running out of memory.
bool TextureCacheIsFull(const std::vector<TexInfo>& textures, int maxImages, int maxBytes);

// The index in `textures` of the one to drop to make room, or -1 if none may go.
// `nPresetsLoadedTotal`: the engine's count of presets loaded so far, the age a texture gets when
// the preset being loaded uses it; the running preset's textures (the next one blends from it)
// are one younger. Neither may go, nor any in `inUse` (bound to a shader still drawn or being
// loaded). Of the others, the largest goes first, its size weighted to count up to twice as much
// the longer ago it was used.
int PickTextureToEvict(const std::vector<TexInfo>& textures, int nPresetsLoadedTotal, const std::vector<const ID3D11Resource*>& inUse);
