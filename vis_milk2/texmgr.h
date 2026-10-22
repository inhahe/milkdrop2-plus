/*
  LICENSE
  -------
  Copyright 2005-2013 Nullsoft, Inc.
  Copyright 2021-2024 Jimmy Cassis
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

#ifndef GEISS_TEXTURE_MANAGER
#define GEISS_TEXTURE_MANAGER

#include "md_defines.h"
#include "d3d11shim.h"
#include "album_art.h"
#ifdef NS_EEL2
#include <eel2/ns-eel.h>
#else
#include <projectm-eval/ns-eel2-shim/ns-eel.h>
#endif

// What a sprite's code is given to place it by: inputs set before its init code and each frame
// (texmgr::SetPlacementInputs).
struct SpritePlacement
{
    double canvasAspect;   // the canvas's width / height: `aspect`
    AlbumArtPlacement art; // the album art settings (PlaceAlbumArt): `artx`, `arty`, `artsize`
};

#define NUM_TEX 16
#define TEXMGR_ERROR_MASK 0x0F
#define TEXMGR_ERR_SUCCESS 0
#define TEXMGR_ERR_BAD_INDEX 1

//#define TEXMGR_ERR_OPENING 2
//#define TEXMGR_ERR_IMAGE_NOT_24_BIT 3
//#define TEXMGR_ERR_IMAGE_TOO_LARGE 4
//#define TEXMGR_ERR_CREATESURFACE_FAILED 5
//#define TEXMGR_ERR_LOCKSURFACE_FAILED 6
//#define TEXMGR_ERR_CORRUPT_JPEG 7

#define TEXMGR_ERR_FORMAT 8
#define TEXMGR_ERR_BADFILE 9
#define TEXMGR_ERR_OUTOFMEM 10
#define TEXMGR_WARNING_MASK 0xF0
#define TEXMGR_WARN_ERROR_IN_INIT_CODE 0x10
#define TEXMGR_WARN_ERROR_IN_REG_CODE 0x20

typedef struct
{
    ID3D11Resource* pSurface;
    int img_w, img_h;

    //int tex_w, tex_h;
    //float scale_x, scale_y; // the factors by which the original image was squished to become (img_w x img_h) texels in size.
    //DDPIXELFORMAT ddpf;

    wchar_t szFileName[512];
    float fStartTime;
    int nStartFrame;
    int nUserData;

    // Expressions
    char m_szExpr[8192];
    NSEEL_CODEHANDLE m_codehandle;

    // Expression inputs
    double *var_time, *var_frame, *var_fps, *var_progress;
    double *var_bass, *var_bass_att, *var_mid, *var_mid_att, *var_treb, *var_treb_att;
    double* var_aspect;    // the canvas's width / height, and
    double* var_imgaspect; // the image's: both set before the init code and each frame, to place by,
    double *var_artx, *var_arty, *var_artsize; // as are the album art settings (SpritePlacement::art)

    // Expression outputs
    double *var_x, *var_y;
    double *var_sx, *var_sy, *var_rot, *var_flipx, *var_flipy;
    double *var_r, *var_g, *var_b, *var_a;
    double *var_blendmode;
    double *var_repeatx, *var_repeaty;
    double *var_done, *var_burn;
    NSEEL_VMCTX tex_eel_ctx;
} td_tex;

class texmgr
{
  public:
    texmgr();
    ~texmgr();

    // Readies the empty sprite slots for textures made on `lpDD`, and returns true; or, while they
    // belong to it already (until Finish, which the device's end calls for), keeps the sprites as
    // they are and returns false, so that they outlive a reallocation of the engine's other
    // resources (a resize, a change of settings, a window swap).
    bool Init(D3D11Shim* lpDD);
    // Sets slot `iSlot`'s placement inputs: `aspect`, `artx`, `arty` and `artsize` from
    // `placement`, and `imgaspect` from its image.
    void SetPlacementInputs(int iSlot, const SpritePlacement& placement);
    // `placement`: what the init code is given to place the sprite by.
    int LoadTex(wchar_t* szFilename, int iSlot, char* szInitCode, char* szCode, float time, uint32_t frame, unsigned int ck, const SpritePlacement& placement);
    int LoadTex(std::vector<uint8_t> rawdata, int iSlot, char* szInitCode, char* szCode, float time, uint32_t frame, unsigned int ck, const SpritePlacement& placement);
    void KillTex(int iSlot);
    void Finish();

    td_tex m_tex[NUM_TEX];

  protected:
    //bool TryCreateDDrawSurface(int iSlot, int w, int h);
    void FreeVars(int iSlot);
    void FreeCode(int iSlot);
    void RegisterBuiltInVariables(int iSlot);
    bool RunInitCode(int iSlot, char* szInitCode, const SpritePlacement& placement);
    bool RecompileExpressions(int iSlot);
    void StripLinefeedCharsAndComments(char* src, char* dest);

    D3D11Shim* m_lpDD;
};

#endif