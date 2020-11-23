package texture

import (
	"fmt"
	"image"
	"image/draw"
	"math"
)

// MemSize is the maximum size in memory of a texture.
const MemSize = 4096

// ToRGBA converts an image to RGBA format.
func ToRGBA(im image.Image) *image.RGBA {
	if ri, ok := im.(*image.RGBA); ok {
		return ri
	}
	b := im.Bounds()
	ri := image.NewRGBA(b)
	draw.Draw(ri, b, im, b.Min, draw.Src)
	return ri
}

var srgbToLinear [256]uint16

func init() {
	for i := 0; i < 256; i++ {
		x := (float64(i) + 0.5) * (1.0 / 256)
		if x < 0.04045 {
			x *= 1.0 / 12.92
		} else {
			x = math.Pow((x+0.055)*(1.0/1.055), 2.4)
		}
		x *= 1 << 16
		srgbToLinear[i] = uint16(x)
	}
}

// ToRGBA16 converts an RGBA image to a linear RGBA64 format.
func ToRGBA16(im *image.RGBA) *image.RGBA64 {
	b := im.Rect
	xsize := b.Max.X - b.Min.X
	ysize := b.Max.Y - b.Min.Y
	ri := image.NewRGBA64(b)
	for y := 0; y < ysize; y++ {
		irow := im.Pix[y*im.Stride : y*im.Stride+xsize*4 : y*im.Stride+xsize*4]
		orow := ri.Pix[y*ri.Stride : y*ri.Stride+xsize*8 : y*ri.Stride+xsize*8]
		for x := 0; x < xsize; x++ {
			ipix := irow[x*4 : x*4+4 : x*4+4]
			opix := orow[x*8 : x*8+8 : x*8+8]
			r := srgbToLinear[ipix[0]]
			g := srgbToLinear[ipix[1]]
			b := srgbToLinear[ipix[2]]
			a := ipix[3]
			opix[0] = byte(r >> 8)
			opix[1] = byte(r)
			opix[2] = byte(g >> 8)
			opix[3] = byte(g)
			opix[4] = byte(b >> 8)
			opix[5] = byte(b)
			opix[6] = a
			opix[7] = 0x80
		}
	}
	return ri
}

// ToRGBA8 converts an RGBA64 image to RGBA format.
func ToRGBA8(im *image.RGBA64) *image.RGBA {
	b := im.Rect
	xsize := b.Max.X - b.Min.X
	ysize := b.Max.Y - b.Min.Y
	ri := image.NewRGBA(b)
	for y := 0; y < ysize; y++ {
		irow := im.Pix[y*im.Stride : y*im.Stride+xsize*8 : y*im.Stride+xsize*8]
		orow := ri.Pix[y*ri.Stride : y*ri.Stride+xsize*4 : y*ri.Stride+xsize*4]
		for x := 0; x < xsize; x++ {
			ipix := irow[x*8 : x*8+8 : x*8+8]
			opix := orow[x*4 : x*4+4 : x*4+4]
			opix[0] = ipix[0]
			opix[1] = ipix[2]
			opix[2] = ipix[4]
			opix[3] = ipix[6]
		}
	}
	return ri
}

// ToSizedFormat changes the values in an image to be within range for a texture
// format and rescales them to 0-255. For I and IA formats, the red channel is
// used for intensity, and it is copied to the green and blue channels. For the
// I format, it is also copied to alpha. CI formats are not supported.
func ToSizedFormat(f SizedFormat, im *image.RGBA) error {
	bits, err := f.ChannelBits()
	if err != nil {
		return err
	}
	sx := im.Rect.Max.X - im.Rect.Min.X
	sy := im.Rect.Max.Y - im.Rect.Min.Y
	pix := im.Pix
	ss := im.Stride
	for c, nbits := range bits {
		switch {
		case nbits >= 8:
		case nbits <= 0:
			for y := 0; y < sy; y++ {
				for x := 0; x < sx; x++ {
					pix[y*ss+x*4+c] = pix[y*ss+x*4]
				}
			}
		default:
			var mul uint32
			for n := uint32(1) << (16 - nbits); n >= 0x100; n >>= nbits {
				mul |= n
			}
			shift := 8 - nbits
			for y := 0; y < sy; y++ {
				for x := 0; x < sx; x++ {
					pix[y*ss+x*4+c] = byte(((uint32(pix[y*ss+x*4+c]) >> shift) * mul) >> 8)
				}
			}
		}
	}
	return nil
}

// Pack packs a texture into binary data.
func Pack(f SizedFormat, im *image.RGBA) ([]byte, error) {
	sx := im.Rect.Max.X - im.Rect.Min.X
	sy := im.Rect.Max.Y - im.Rect.Min.Y
	ss := im.Stride
	switch f.Size {
	case Size32:
		ds := sx * 4
		r := make([]byte, sy*ds)
		if f.Format != RGBA {
			return nil, fmt.Errorf("invalid format: %s", f)
		}
		for y := 0; y < sy; y++ {
			sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
			dr := r[y*ds : (y+1)*ds : (y+1)*ds]
			copy(dr, sr)
		}
		return r, nil
	case Size16:
		ds := sx * 2
		r := make([]byte, sy*ds)
		switch f.Format {
		case RGBA:
			for y := 0; y < sy; y++ {
				sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
				dr := r[y*ds : (y+1)*ds : (y+1)*ds]
				for x := 0; x < sx; x++ {
					c := (((uint32(sr[x*4]) >> 3) & 31) << 11) |
						(((uint32(sr[x*4+1]) >> 3) & 31) << 6) |
						(((uint32(sr[x*4+2]) >> 3) & 31) << 1) |
						((uint32(sr[x*4+3]) >> 7) & 1)
					dr[x*2] = byte(c >> 8)
					dr[x*2+1] = byte(c)
				}
			}
		case IA:
			for y := 0; y < sy; y++ {
				sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
				dr := r[y*ds : (y+1)*ds : (y+1)*ds]
				for x := 0; x < sx; x++ {
					dr[x*2] = sr[x*4]
					dr[x*2+1] = sr[x*4+3]
				}
			}
		default:
			return nil, fmt.Errorf("invalid format: %s", f)
		}
		return r, nil
	case Size8:
		ds := sx
		r := make([]byte, sy*ds)
		switch f.Format {
		case IA:
			for y := 0; y < sy; y++ {
				sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
				dr := r[y*ds : (y+1)*ds : (y+1)*ds]
				for x := 0; x < sx; x++ {
					dr[x] = (sr[x*4] & 0xf0) | (sr[x*4+3] >> 4)
				}
			}
		case I:
			for y := 0; y < sy; y++ {
				sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
				dr := r[y*ds : (y+1)*ds : (y+1)*ds]
				for x := 0; x < sx; x++ {
					dr[x] = sr[x*4]
				}
			}
		default:
			return nil, fmt.Errorf("invalid format: %s", f)
		}
		return r, nil
	case Size4:
		if sx&1 != 0 {
			return nil, fmt.Errorf("invalid width for 4-bit texture: %d, width is not even", sx)
		}
		ds := sx / 2
		r := make([]byte, sy*ds)
		tmp := make([]byte, sx)
		for y := 0; y < sy; y++ {
			sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
			dr := r[y*ds : (y+1)*ds : (y+1)*ds]
			switch f.Format {
			case IA:
				for x := 0; x < sx; x++ {
					tmp[x] = (((sr[x*4] >> 5) & 7) << 1) | ((sr[x*4+3] >> 7) & 1)
				}
			case I:
				for x := 0; x < sx; x++ {
					tmp[x] = sr[x*4] >> 4
				}
			default:
				return nil, fmt.Errorf("invalid format: %s", f)
			}
			for x := 0; x < sx/2; x++ {
				dr[x] = (tmp[x*2] << 4) | (tmp[x*2+1] & 15)
			}
		}
		return r, nil
	default:
		return nil, fmt.Errorf("invalid format: %s", f)
	}
}
