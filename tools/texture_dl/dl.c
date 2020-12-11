#include "game/n64/texture_dl/dl.h"

static const Gfx texture_rgba16_32x32[] = {
    gsDPPipeSync(),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsDPSetTextureLOD(G_TL_LOD),
    gsDPSetTextureFilter(G_TF_BILERP),
#include "game/n64/texture_dl/rgba16_32x32.h"
    gsSPEndDisplayList(),
};

static const Gfx texture_i4_64x64[] = {
    gsDPPipeSync(),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsDPSetTextureLOD(G_TL_LOD),
    gsDPSetTextureFilter(G_TF_BILERP),
#include "game/n64/texture_dl/i4_64x64.h"
    gsSPEndDisplayList(),
};

const Gfx *const texture_dls[3] = {
    [1] = texture_rgba16_32x32,
    [2] = texture_i4_64x64,
};
