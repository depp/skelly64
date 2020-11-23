package texture

import (
	"errors"
	"fmt"
	"image"
)

// Scale returns an image scaled down by the given number of factors of two in
// the X and Y directions.
func Scale(im *image.RGBA64, xbits, ybits int) (*image.RGBA64, error) {
	if xbits < 0 || 31 < xbits || ybits < 0 || 31 < ybits {
		return nil, fmt.Errorf("invalid scale: (%d, %d)", xbits, ybits)
	}
	xsize := im.Rect.Max.X - im.Rect.Min.X
	ysize := im.Rect.Max.Y - im.Rect.Min.Y
	xsize2 := xsize >> xbits
	ysize2 := ysize >> ybits
	if xsize2<<xbits != xsize || ysize2<<ybits != ysize {
		return nil, fmt.Errorf("image with size (%d, %d) not divisible by (%d, %d)",
			xsize, ysize, 1<<xbits, 1<<ybits)
	}
	out := image.NewRGBA64(image.Rectangle{
		Max: image.Point{X: xsize2, Y: ysize2},
	})
	for y := 0; y < ysize2; y++ {
		for x := 0; x < xsize2; x++ {
			var r, g, b, a uint32
			for yy := 0; yy < (1 << ybits); yy++ {
				offset := im.Stride*((y<<ybits)+yy) + (x << (xbits + 3))
				n := 1 << (xbits + 3)
				slice := im.Pix[offset : offset+n : offset+n]
				for xx := 0; xx < (1 << xbits); xx++ {
					pix := slice[xx*8 : xx*8+8 : xx*8+8]
					r += (uint32(pix[0]) << 8) | uint32(pix[1])
					g += (uint32(pix[2]) << 8) | uint32(pix[3])
					b += (uint32(pix[4]) << 8) | uint32(pix[5])
					a += (uint32(pix[6]) << 8) | uint32(pix[7])
				}
			}
			r >>= xbits + ybits
			g >>= xbits + ybits
			b >>= xbits + ybits
			a >>= xbits + ybits
			pix := out.Pix[y*out.Stride+x*8 : y*out.Stride+x*8+8 : y*out.Stride+x*8+8]
			pix[0] = uint8(r >> 8)
			pix[1] = uint8(r)
			pix[2] = uint8(g >> 8)
			pix[3] = uint8(g)
			pix[4] = uint8(b >> 8)
			pix[5] = uint8(b)
			pix[6] = uint8(a >> 8)
			pix[7] = uint8(a)
		}
	}
	return out, nil
}

// AutoScale scales the image to fit within the given number of bytes. The
// number of bits per pixel is given in 'bits'.
func AutoScale(im *image.RGBA64, maxbytes, bits int) (*image.RGBA64, error) {
	b := im.Rect
	xsize := b.Max.X - b.Min.X
	ysize := b.Max.Y - b.Min.Y
	var xscale, yscale int
	for {
		stride := ((xsize * bits >> 3) + 7) &^ 7
		bytes := stride * ysize
		if bytes <= maxbytes {
			return Scale(im, xscale, yscale)
		}
		if xsize == 1 && ysize == 1 {
			return nil, errors.New("failed to autoscale")
		}
		if xsize > 1 {
			xsize >>= 1
			xscale++
		}
		if ysize > 1 {
			ysize >>= 1
			yscale++
		}
	}
}
