package texture

import (
	"errors"
	"fmt"
	"image"
	"image/draw"
	"math"
)

// MemSize is the maximum size in memory of a texture.
const MemSize = 4096

// A Layout describes the texture layout.
type Layout uint32

const (
	// Linear is the ordinary linear texture layout, with rows of pixels packed
	// after each other.
	Linear Layout = iota

	// Native is the texture layout that the texture unit on the RDP uses. The
	// exact layout depends on the size, and is described in section 13.8 of the
	// programming manual. In general, texels are reordered so that any 2x2
	// block of texels can be fetched in a single cycle.
	Native
)

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

// swizzle converts between linear and native layouts for 4-bit, 8-bit, and
// 16-bit per pixel size textures.
func swizzle(data []byte, stride int) {
	if stride&7 != 0 {
		panic("swizzle requires stride which is a multiple of 8")
	}
	// Iterate over the odd-numbered rows.
	var tmp [4]byte
	for len(data) >= stride*2 {
		row := data[stride : stride*2 : stride*2]
		data = data[stride*2:]
		for len(row) >= 8 {
			block := row[0:8:8]
			row = row[8:]
			copy(tmp[:], block[:4])
			copy(block[:4], block[4:])
			copy(block[4:], tmp[:])
		}
	}
}

// Pack packs a texture into binary data.
func Pack(im *image.RGBA, f SizedFormat, layout Layout) ([]byte, error) {
	sx := im.Rect.Max.X - im.Rect.Min.X
	sy := im.Rect.Max.Y - im.Rect.Min.Y
	ss := im.Stride
	switch f.Size {
	case Size32:
		if layout != Linear {
			// Reason for this: unlike smaller formats, 32 bpp textures must be
			// split across the low & high banks of TMEM.
			return nil, errors.New("native layout unsupported for 32 bpp textures")
		}
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
		if layout == Native {
			ds = (ds + 7) &^ 7
		}
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
		if layout == Native {
			swizzle(r, ds)
		}
		return r, nil
	case Size8:
		ds := sx
		if layout == Native {
			ds = (ds + 7) &^ 7
		}
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
		if layout == Native {
			swizzle(r, ds)
		}
		return r, nil
	case Size4:
		if sx&1 != 0 {
			return nil, fmt.Errorf("invalid width for 4-bit texture: %d, width is not even", sx)
		}
		ds := sx / 2
		if layout == Native {
			ds = (ds + 7) &^ 7
		}
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
		if layout == Native {
			swizzle(r, ds)
		}
		return r, nil
	default:
		return nil, fmt.Errorf("invalid format: %s", f)
	}
}
