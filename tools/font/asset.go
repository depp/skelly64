package main

import (
	"encoding/binary"
	"fmt"
	"math"
	"sort"
	"thornmarked/tools/texture"
)

func makeAssetCharmap(fn *font) ([]byte, error) {
	const recSize = 4
	chars := make([]int, 0, len(fn.charmap))
	for c := range fn.charmap {
		chars = append(chars, int(c))
	}
	sort.Ints(chars)
	out := make([]byte, recSize*len(chars))
	for i, c := range chars {
		g := fn.charmap[uint32(c)]
		if c > math.MaxUint16 {
			return nil, fmt.Errorf("character does not fit in 16 bits: %#x", c)
		}
		rec := out[i*recSize : (i+1)*recSize : (i+1)*recSize]
		binary.BigEndian.PutUint16(rec[:2], uint16(c))
		binary.BigEndian.PutUint16(rec[2:], uint16(g))
	}
	return out, nil
}

func makeAssetGlyphs(fn *font) ([]byte, error) {
	const recSize = 8
	out := make([]byte, len(fn.glyphs)*recSize)
	for i, g := range fn.glyphs {
		copy(out[i*recSize:(i+1)*recSize:(i+1)*recSize], []byte{
			byte(g.size[0]),
			byte(g.size[1]),
			byte(-g.center[0]),
			byte(-g.center[1]),
			byte(g.pos[0]),
			byte(g.pos[1]),
			byte(g.texindex),
			byte(g.advance),
		})
	}
	return out, nil
}

func makeAssetTextures(fn *font, tf texture.SizedFormat) ([]byte, error) {
	const recSize = 4
	out := make([]byte, len(fn.textures)*recSize)
	var pad [4]byte
	for i, t := range fn.textures {
		rec := out[i*recSize : (i+1)*recSize : (i+1)*recSize]
		sx := t.Rect.Max.X - t.Rect.Min.X
		sy := t.Rect.Max.Y - t.Rect.Min.Y
		binary.BigEndian.PutUint16(rec[0:2], uint16(sx))
		binary.BigEndian.PutUint16(rec[2:4], uint16(sy))
		d, err := texture.Pack(tf, t)
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
	binary.BigEndian.PutUint16(out[0:2], uint16(len(fn.charmap)))
	binary.BigEndian.PutUint16(out[2:4], uint16(len(fn.glyphs)))
	binary.BigEndian.PutUint16(out[4:6], uint16(len(fn.textures)))
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
	tex, err := makeAssetTextures(fn, tf)
	if err != nil {
		return nil, err
	}
	out = append(out, tex...)
	return out, nil
}
