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

// texture_cache.cpp - Which loaded texture to drop first.

#include "pch.h"
#include "texture_cache.h"

#include <algorithm>
#include <climits>

int TextureSizeInBytes(unsigned w, unsigned h, unsigned d, int bitsPerPixel)
{
    const long long bytes = static_cast<long long>(w) * h * std::max(1u, d) * std::max(0, bitsPerPixel) / 8 + 16384; // plus some overhead
    return static_cast<int>(std::min<long long>(bytes, INT_MAX));
}

bool TextureCacheIsFull(const std::vector<TexInfo>& textures, int maxImages, int maxBytes)
{
    int nImages = 0;
    long long nBytes = 0;
    for (const TexInfo& t : textures)
        if (t.bEvictable && t.texptr)
        {
            ++nImages;
            nBytes += t.nSizeInBytes;
        }
    return (maxImages >= 0 && nImages >= maxImages) || (maxBytes >= 0 && nBytes >= maxBytes);
}

int PickTextureToEvict(const std::vector<TexInfo>& textures, int nPresetsLoadedTotal, const std::vector<const ID3D11Resource*>& inUse)
{
    const auto mayGo = [nPresetsLoadedTotal, &inUse](const TexInfo& t) {
        return t.bEvictable && t.nSizeInBytes > 0 && t.nAge < nPresetsLoadedTotal - 1 &&
               std::find(inUse.begin(), inUse.end(), t.texptr) == inUse.end();
    };

    int minAge = INT_MAX; // least recently used
    int maxAge = INT_MIN; // most recently used
    for (const TexInfo& t : textures)
        if (mayGo(t))
        {
            minAge = std::min(minAge, t.nAge);
            maxAge = std::max(maxAge, t.nAge);
        }

    int best = -1;
    float bestWeight = 0.0f;
    for (size_t i = 0; i < textures.size(); i++)
    {
        const TexInfo& t = textures[i];
        if (!mayGo(t))
            continue;
        const float mult = (maxAge > minAge) ? 1.0f + static_cast<float>(maxAge - t.nAge) / static_cast<float>(maxAge - minAge) : 1.0f;
        const float weight = static_cast<float>(t.nSizeInBytes) * mult;
        if (weight > bestWeight)
        {
            bestWeight = weight;
            best = static_cast<int>(i);
        }
    }
    return best;
}
