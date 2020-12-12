package texture

import (
	"fmt"
	"image"
	"image/png"
	"os"
	"path/filepath"
	"strings"
)

// ReadPNG reads an image file.
func ReadPNG(filename string) (image.Image, error) {
	ext := filepath.Ext(filename)
	if !strings.EqualFold(ext, ".png") {
		return nil, fmt.Errorf("file does not have .png extension: %q", filename)
	}
	fp, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer fp.Close()
	im, err := png.Decode(fp)
	if err != nil {
		return nil, fmt.Errorf("could not decode %q: %w", filename, err)
	}
	return im, nil
}

// IsEmpty returns true if the image contains no pixels with non-zero alpha.
func IsEmpty(im *image.RGBA) bool {
	var alpha byte
	ysz := im.Rect.Dy()
	xsz := im.Rect.Dx()
	for y := 0; y < ysz; y++ {
		off := y * im.Stride
		row := im.Pix[off : off+xsz*4 : off+xsz*4]
		for x := 0; x < xsz; x++ {
			alpha |= row[x*4+3]
		}
	}
	return alpha == 0
}

// Trim returns the smallest subimage of the image containing all pixels with
// nonzero alpha. Returns nil if empty.
func Trim(im *image.RGBA) *image.RGBA {
	ysz := im.Rect.Dy()
	xsz := im.Rect.Dx()
	xmin := xsz
	xmax := 0
	ymin := ysz
	ymax := 0
	for y := 0; y < ysz; y++ {
		off := y * im.Stride
		row := im.Pix[off : off+xsz*4 : off+xsz*4]
		x0 := 0
		x1 := xsz
		for x0 < xsz && row[x0*4+3] == 0 {
			x0++
		}
		for x1 > 0 && row[x1*4-1] == 0 {
			x1--
		}
		if x0 < x1 {
			if x0 < xmin {
				xmin = x0
			}
			if xmax < x1 {
				xmax = x1
			}
			if y < ymin {
				ymin = y
			}
			if ymax < y+1 {
				ymax = y + 1
			}
		}
	}
	if xmin >= xmax || ymin >= ymax {
		return nil
	}
	return im.SubImage(image.Rectangle{
		Min: image.Point{X: xmin, Y: ymin},
		Max: image.Point{X: xmax, Y: ymax},
	}.Add(im.Rect.Min)).(*image.RGBA)
}
