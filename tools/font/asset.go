package main

import (
	"encoding/binary"
	"fmt"
	
	"github.com/depp/skelly64/lib/texture"
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
	var pad [7]byte
	for i, t := range fn.textures {
		out = append(out, pad[:(-len(out))&7]...)
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
	}
	return out, nil
}

func makeAsset(fn *font, tf texture.SizedFormat) ([]byte, error) {
	hdr := make([]byte, 8)
	binary.BigEndian.PutUint16(hdr[0:2], uint16(len(fn.glyphs)))
	binary.BigEndian.PutUint16(hdr[2:4], uint16(len(fn.textures)))
	pos := len(hdr)

	cmap, err := makeAssetCharmap(fn)
	if err != nil {
		return nil, err
	}
	pos += len(cmap)

	glyphs, err := makeAssetGlyphs(fn)
	if err != nil {
		return nil, err
	}
	pos += len(glyphs)

	binary.BigEndian.PutUint32(hdr[4:8], uint32(pos))
	tex, err := makeAssetTextures(fn, tf, uint32(pos))
	if err != nil {
		return nil, err
	}
	pos += len(tex)

	out := make([]byte, 16, 16+pos)
	copy(out, "Font")
	out = append(out, hdr...)
	out = append(out, cmap...)
	out = append(out, glyphs...)
	out = append(out, tex...)

	return out, nil
}
