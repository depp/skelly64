#include "game/n64/texture_dl/dl.h"

static const Gfx texture_rgba16_32x32[] = {
    gsDPPipeSync(),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureLOD(G_TL_LOD),
    gsDPSetTextureFilter(G_TF_BILERP),
#include "game/n64/texture_dl/rgba16_32x32.h"
    gsSPEndDisplayList(),
};

static const Gfx texture_i4_64x64[] = {
    gsDPPipeSync(),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureLOD(G_TL_LOD),
    gsDPSetTextureFilter(G_TF_BILERP),
#include "game/n64/texture_dl/i4_64x64.h"
    gsSPEndDisplayList(),
};

static const Gfx texture_ci4_32x32[] = {
    gsDPPipeSync(),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureLOD(G_TL_LOD),
    gsDPSetTextureFilter(G_TF_BILERP),
#include "game/n64/texture_dl/ci4_32x32.h"
    gsSPEndDisplayList(),
};

const Gfx *const texture_dls[4] = {
    [1] = texture_rgba16_32x32,
    [2] = texture_i4_64x64,
    [3] = texture_ci4_32x32,
};
