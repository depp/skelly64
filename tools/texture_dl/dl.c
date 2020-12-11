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

    // Load data into TMEM (using G_TX_LOADTILE).

    gsDPSetTile(       //
        G_IM_FMT_RGBA, // format
        G_IM_SIZ_16b,  // size
        0,             // size of one row
        0,             // tmem
        G_TX_LOADTILE, // tile
        0,             // palette
        G_TX_NOMIRROR, // cmt
        0,             // maskt
        G_TX_NOLOD,    // shiftt
        G_TX_NOMIRROR, // cms
        0,             // masks
        G_TX_NOLOD),   // shifts

    gsDPLoadSync(),

    gsDPLoadBlock(     //
        G_TX_LOADTILE, // tile
        0,             // uls
        0,             // ult
        1371,          // lrs
        0),            // dxt

    gsDPPipeSync(),

    // Tile: format, size, length of row in 8-byte units, TMEM address, tile,
    // palette, cmt, maskt, shiftt, cms, masks, shifts.
    //
    // TileSize: tile, uls, ult, lrs, lrt
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 4, 0, 0, 0, 0, 6, 0, 0, 6, 0),
    gsDPSetTileSize(0, 0, 0, 63 << G_TEXTURE_IMAGE_FRAC,
                    63 << G_TEXTURE_IMAGE_FRAC),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 2, 256, 1, 0, 0, 5, 1, 0, 5, 1),
    gsDPSetTileSize(1, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                    31 << G_TEXTURE_IMAGE_FRAC),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 1, 320, 2, 0, 0, 4, 2, 0, 4, 2),
    gsDPSetTileSize(2, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                    15 << G_TEXTURE_IMAGE_FRAC),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 1, 336, 3, 0, 0, 3, 3, 0, 3, 3),
    gsDPSetTileSize(3, 0, 0, 7 << G_TEXTURE_IMAGE_FRAC,
                    7 << G_TEXTURE_IMAGE_FRAC),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 1, 344, 4, 0, 0, 2, 4, 0, 2, 4),
    gsDPSetTileSize(4, 0, 0, 3 << G_TEXTURE_IMAGE_FRAC,
                    3 << G_TEXTURE_IMAGE_FRAC),
    gsDPSetTile(G_IM_FMT_I, G_IM_SIZ_4b, 1, 342, 5, 0, 0, 1, 5, 0, 1, 5),
    gsDPSetTileSize(5, 0, 0, 1 << G_TEXTURE_IMAGE_FRAC,
                    1 << G_TEXTURE_IMAGE_FRAC),

    gsSPEndDisplayList(),
};

const Gfx *const texture_dls[3] = {
    [1] = texture_rgba16_32x32,
    [2] = texture_i4_64x64,
};
