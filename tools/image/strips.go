package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"os"

	"thornmarked/tools/texture"
)

type rowSpan struct {
	start, end int
}

// rowSpans returns the span of nonzero pixels in each row.
func rowSpans(img *image.RGBA) []rowSpan {
	xsize := img.Rect.Dx()
	ysize := img.Rect.Dy()
	spans := make([]rowSpan, ysize)
	for y := 0; y < ysize; y++ {
		off := y * img.Stride
		row := img.Pix[off : off+xsize*4 : off+xsize*4]
		var start int
		for start < xsize && (row[start*4]|row[start*4+1]|row[start*4+2]|row[start*4+3]) == 0 {
			start++
		}
		end := xsize
		for 0 < end && (row[end*4-1]|row[end*4-2]|row[end*4-3]|row[end*4-4]) == 0 {
			end--
		}
		spans[y] = rowSpan{start, end}
	}
	return spans
}

func rectBytes(r image.Rectangle, size int) int {
	// Size is in bits, convert to bytes, pad to 64-bit boundaries.
	stride := ((r.Dx()*size + 63) &^ 63) >> 3
	return stride * r.Dy()
}

func makeStripRects1(spans []rowSpan, size int) ([]image.Rectangle, error) {
	const limit = tmemSize
	var rs []image.Rectangle
	for y := 0; y < len(spans); {
		r := image.Rectangle{
			Min: image.Point{X: spans[y].start, Y: y},
			Max: image.Point{X: spans[y].end, Y: y + 1},
		}
		rsize := rectBytes(r, size)
		if rsize > limit {
			return nil, fmt.Errorf(
				"image too wide, width=%d, bytes=%d, maxbytes=%d",
				r.Dx(), rsize, limit)
		}
		for r.Max.Y < len(spans) {
			r2 := r
			s := spans[r2.Max.Y]
			if s.start < r2.Min.X {
				r2.Min.X = s.start
			}
			if s.end > r2.Max.X {
				r2.Max.X = s.end
			}
			r2.Max.Y++
			if rectBytes(r2, size) > limit {
				break
			}
			r = r2
		}
		rs = append(rs, r)
		y = r.Max.Y
	}
	return rs, nil
}

// makeStrip divides an image into strips that fit in TMEM.
func makeStripRects(img *image.RGBA, size int) ([]image.Rectangle, error) {
	spans := rowSpans(img)
	var strips []image.Rectangle
	y := 0
	x0 := img.Rect.Min.X
	ysize := img.Rect.Dy()
	for {
		// Find a range of non-empty rows.
		for y < ysize && spans[y].end == 0 {
			y++
		}
		y0 := y
		for y < ysize && spans[y].end != 0 {
			y++
		}
		if y == y0 {
			break
		}

		// Break the range into ranges that fit in TMEM.
		rs, err := makeStripRects1(spans[y0:y], size)
		if err != nil {
			return nil, err
		}
		for _, r := range rs {
			strips = append(strips, r.Add(image.Point{X: x0, Y: y0}))
		}
	}
	return strips, nil
}

// makeStrips converts an image to a strip image, for displaying large images on-screen at full-size.
func makeStrips(opts *options, img *image.RGBA) ([]byte, error) {
	if err := texture.ToSizedFormat(opts.format, img, opts.dithering); err != nil {
		return nil, fmt.Errorf("could not convert to %s: %v", opts.format, err)
	}
	psize := opts.format.Size.Size()
	strips, err := makeStripRects(img, psize)
	if err != nil {
		return nil, fmt.Errorf("could not divide image into strips: %v", err)
	}
	if len(strips) == 0 {
		return nil, errors.New("empty image")
	}

	if false {
		for _, r := range strips {
			fmt.Fprintf(os.Stderr, "Y: %d-%d; X: %d-%d\n",
				r.Min.Y, r.Max.Y, r.Min.X, r.Max.X)
		}
	}

	const (
		magicsz = 16
		rsz     = 12
	)
	hsz := 4 + rsz*len(strips)
	pos := hsz
	type loc struct {
		pos  int
		size int
	}
	locs := make([]loc, len(strips))
	for i, r := range strips {
		pos = (pos + 7) &^ 7
		size := ((r.Dx()*psize + 7) >> 3) * r.Dy()
		locs[i] = loc{pos: pos, size: size}
		pos += size
	}
	data := make([]byte, magicsz+pos)
	copy(data, "StripImage")
	d := data[magicsz:]
	e := binary.BigEndian
	e.PutUint32(d, uint32(len(strips)))
	for i, r := range strips {
		loc := locs[i]
		off := 4 + rsz*i
		rd := d[off : off+rsz : off+rsz]
		e.PutUint16(rd, uint16(r.Min.X))
		e.PutUint16(rd[2:], uint16(r.Min.Y))
		e.PutUint16(rd[4:], uint16(r.Dx()))
		e.PutUint16(rd[6:], uint16(r.Dy()))
		e.PutUint32(rd[8:], uint32(loc.pos))
		td, err := texture.Pack(
			img.SubImage(r).(*image.RGBA), opts.format, texture.Linear)
		if err != nil {
			return nil, fmt.Errorf("could not pack strip: %v", err)
		}
		if len(td) != loc.size {
			panic("BAD SIZE")
		}
		copy(d[loc.pos:], td)
	}
	return data, nil
}
