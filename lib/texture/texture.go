package texture

import (
	"fmt"
	"image"
	"image/draw"
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

// ToSizedFormat changes the values in an image to be within range for a texture
// format. For I and IA formats, the red channel is used for intensity, and the
// green and blue channels are zeroed. CI formats are not supported.
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
					pix[y*ss+x*4+c] = 0
				}
			}
		default:
			shift := 8 - nbits
			for y := 0; y < sy; y++ {
				for x := 0; x < sx; x++ {
					pix[y*ss+x*4+c] = pix[y*ss+x*4+c] >> shift
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
					c := ((uint32(sr[x*4]) & 31) << 11) |
						((uint32(sr[x*4+1]) & 31) << 6) |
						((uint32(sr[x*4+2]) & 31) << 1) |
						(uint32(sr[x*4+3]) & 1)
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
					dr[x] = (sr[x*4] << 4) | (sr[x*4+3] & 15)
				}
			}
		case I:
			for y := 0; y < sy; y++ {
				sr := im.Pix[y*ss : y*ss+sx*4 : y*ss+sx*4]
				dr := r[y*ds : (y+1)*ds : (y+1)*ds]
				for x := 0; x < sx; x++ {
					dr[x] = sr[x*4] << 4
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
					tmp[x] = ((sr[x*4] & 7) << 1) | (sr[x*4+3] & 1)
				}
			case I:
				for x := 0; x < sx; x++ {
					tmp[x] = sr[x*4]
				}
			default:
				return nil, fmt.Errorf("invalid format: %s", f)
			}
			for x := 0; x < sx/2; x++ {
				dr[y*ds+x] = (tmp[x*2] << 4) | (tmp[x*2+1] & 15)
			}
		}
		return r, nil
	default:
		return nil, fmt.Errorf("invalid format: %s")
	}
}
