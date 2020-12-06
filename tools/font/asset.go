package main

import (
	"encoding/binary"
	"fmt"
	"thornmarked/tools/texture"
)

const ()

// makeAssetCharmap makes the character map array for the asset.
func makeAssetCharmap(fn *font) ([]byte, error) {
	out := make([]byte, 256)
	for i, c := range fn.charmap {
		if i >= uint32(len(out)) {
			return nil, fmt.Errorf("character value too high, character = %d", i)
		}
		if c > 255 {
			return nil, fmt.Errorf("glyph index too high, index = %d", c)
		}
		out[i] = byte(c)
	}
	return out, nil
}

// makeAssetGlyphs makes the glyph array for the asset.
func makeAssetGlyphs(fn *font) ([]byte, error) {
	const recSize = 8
	out := make([]byte, len(fn.glyphs)*recSize)
	for i, g := range fn.glyphs {
		off := i * recSize
		rec := out[off : off+recSize : off+recSize]
		if g.size[0] > 0 && g.size[1] > 0 {
			copy(rec, []byte{
				byte(g.size[0]),
				byte(g.size[1]),
				byte(g.center[0]),
				byte(-g.center[1]),
				byte(g.pos[0]),
				byte(g.pos[1]),
				byte(g.texindex),
				byte(g.advance),
			})
		} else {
			rec[7] = byte(g.advance)
		}
	}
	return out, nil
}

// makeAssetTextures makes the texture array for the asset.
func makeAssetTextures(fn *font, tf texture.SizedFormat, offset uint32) ([]byte, error) {
	const recSize = 12
	out := make([]byte, len(fn.textures)*recSize)
	var pad [4]byte
	for i, t := range fn.textures {
		off := i * recSize
		rec := out[off : off+recSize : off+recSize]
		sx := t.Rect.Max.X - t.Rect.Min.X
		sy := t.Rect.Max.Y - t.Rect.Min.Y
		binary.BigEndian.PutUint16(rec[0:2], uint16(sx))
		binary.BigEndian.PutUint16(rec[2:4], uint16(sy))
		binary.BigEndian.PutUint16(rec[4:6], uint16(tf.Format.Enum()))
		binary.BigEndian.PutUint16(rec[6:8], uint16(tf.Size.Enum()))
		binary.BigEndian.PutUint32(rec[8:12], offset+uint32(len(out)))
		d, err := texture.Pack(t, tf, texture.Linear)
		if err != nil {
			return nil, err
		}
		if len(d) > texture.MemSize {
			return nil, fmt.Errorf(
				"texture with size %dx%d and format %s is %d bytes, which is larger than the maximum size of %d",
				sx, sy, tf, len(d), texture.MemSize)
		}
		out = append(out, d...)
		out = append(out, pad[:(-len(out))&3]...)
	}
	return out, nil
}

func makeAsset(fn *font, tf texture.SizedFormat) ([]byte, error) {
	out := make([]byte, 8)
	binary.BigEndian.PutUint16(out[0:2], uint16(len(fn.glyphs)))
	binary.BigEndian.PutUint16(out[2:4], uint16(len(fn.textures)))

	cmap, err := makeAssetCharmap(fn)
	if err != nil {
		return nil, err
	}
	out = append(out, cmap...)

	glyphs, err := makeAssetGlyphs(fn)
	if err != nil {
		return nil, err
	}
	out = append(out, glyphs...)

	binary.BigEndian.PutUint32(out[4:8], uint32(len(out)))
	tex, err := makeAssetTextures(fn, tf, uint32(len(out)))
	if err != nil {
		return nil, err
	}
	out = append(out, tex...)

	return out, nil
}
