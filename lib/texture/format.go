package texture

import (
	"fmt"
	"strconv"
	"strings"
)

// A Format is an unsized pixel format.
type Format uint32

const (
	// UnknownFormat is an unknown or missing format.
	UnknownFormat Format = iota
	// RGBA is RGB color with alpha.
	RGBA
	// CI is indexed color.
	CI
	// IA is intensity (grayscale) with alpha.
	IA
	// I is intensity only (grayscale) without alpha.
	I
)

var formats = [...]struct {
	name string
	enum uint32
}{
	RGBA: {name: "RGBA", enum: 0},
	CI:   {name: "CI", enum: 2},
	IA:   {name: "IA", enum: 3},
	I:    {name: "I", enum: 4},
}

// String returns the name of the format.
func (f Format) String() (s string) {
	i := uint32(f)
	if i < uint32(len(formats)) {
		s = formats[i].name
	}
	if s == "" {
		s = strconv.FormatUint(uint64(i), 10)
	}
	return
}

// Set sets the format to a string value.
func (f *Format) Set(s string) error {
	for i, n := range formats {
		if strings.EqualFold(s, n.name) {
			*f = Format(i)
			return nil
		}
	}
	return fmt.Errorf("unknown format: %q", s)
}

// Enum returns the N64 enum value for this pixel format.
func (f Format) Enum() uint32 {
	i := uint32(f)
	if 0 < i && i < uint32(len(formats)) {
		return formats[i].enum
	}
	panic("invalid pixel format")
}

// PixelSize is a texture pixel size.
type PixelSize uint32

const (
	// UnknownSize is an unknown or invalid size.
	UnknownSize PixelSize = iota
	// Size32 is 32 bits per pixel.
	Size32
	// Size16 is 32 bits per pixel.
	Size16
	// Size8 is 32 bits per pixel.
	Size8
	// Size4 is 32 bits per pixel.
	Size4
)

var pixelsizes = [...]struct {
	size uint32
	enum uint32
}{
	Size32: {size: 32, enum: 3},
	Size16: {size: 16, enum: 2},
	Size8:  {size: 8, enum: 1},
	Size4:  {size: 4, enum: 0},
}

// String returns the name of the pixel size.
func (s PixelSize) String() (st string) {
	i := uint32(s)
	if i < uint32(len(pixelsizes)) {
		if sz := pixelsizes[i].size; sz != 0 {
			return strconv.FormatUint(uint64(sz), 10)
		}
	}
	return "PixelSize(" + strconv.FormatUint(uint64(i), 10) + ")"
}

// Set sets the pixel size to a string value.
func (s *PixelSize) Set(st string) error {
	sz, err := strconv.ParseUint(st, 10, 32)
	if err != nil {
		return err
	}
	for i, isz := range pixelsizes {
		if isz.size == uint32(sz) {
			*s = PixelSize(i)
			return nil
		}
	}
	return fmt.Errorf("unsupported pixel size: %d", sz)
}

// Size returns the size of the pixel format in bits.
func (s PixelSize) Size() int {
	i := uint32(s)
	if i < uint32(len(pixelsizes)) {
		return int(pixelsizes[i].size)
	}
	return 0
}

// Enum returns the N64 enum value for this pixel size.
func (s PixelSize) Enum() uint32 {
	i := uint32(s)
	if 0 < i && i < uint32(len(pixelsizes)) {
		return pixelsizes[i].enum
	}
	panic("invalid pixel size")
}

// A SizedFormat is a combination pixel format and size.
type SizedFormat struct {
	Format Format
	Size   PixelSize
}

// String converts the sized format to a string.
func (f *SizedFormat) String() (s string) {
	return f.Format.String() + "." + f.Size.String()
}

// Set sets the sized format to a string value.
func (f *SizedFormat) Set(s string) error {
	i := strings.IndexByte(s, '.')
	if i == -1 {
		return fmt.Errorf("invalid format: expected 'format.size', could not find '.': %q", s)
	}
	var t SizedFormat
	if err := t.Format.Set(s[:i]); err != nil {
		return err
	}
	if err := t.Size.Set(s[i+1:]); err != nil {
		return err
	}
	switch t.Format {
	case RGBA:
		if t.Size != Size32 && t.Size != Size16 {
			return fmt.Errorf("invalid size %s for RGBA, only 32 and 16 are supported", t.Size)
		}
	case CI:
		if t.Size != Size8 && t.Size != Size4 {
			return fmt.Errorf("invalid size %s for CI, only 8 and 4 are supported", t.Size)
		}
	case IA:
		if t.Size != Size16 && t.Size != Size8 && t.Size != Size4 {
			return fmt.Errorf("invalid size %s for IA, only 16, 8, and 4 are supported", t.Size)
		}
	case I:
		if t.Size != Size8 && t.Size != Size4 {
			return fmt.Errorf("invalid size %s for I, only 8 and 4 are supported", t.Size)
		}
	}
	*f = t
	return nil
}

// ChannelBits returns the number of bits in each channel. Four channels are
// always returned. For I and IA textures, the red channel is used as the
// intensity channel, and the blue and green channels are given zero bits.
func (f SizedFormat) ChannelBits() (bits [4]int, err error) {
	switch f.Format {
	case RGBA:
		switch f.Size {
		case Size32:
			bits = [4]int{8, 8, 8, 8}
		case Size16:
			bits = [4]int{5, 5, 5, 1}
		default:
			return bits, fmt.Errorf("format RGBA does not support size: %s", f.Size)
		}
	case IA:
		switch f.Size {
		case Size16:
			bits = [4]int{8, 0, 0, 8}
		case Size8:
			bits = [4]int{4, 0, 0, 4}
		case Size4:
			bits = [4]int{3, 0, 0, 1}
		default:
			return bits, fmt.Errorf("format IA does not support size: %s", f.Size)
		}
	case I, CI:
		switch f.Size {
		case Size8:
			bits = [4]int{8, 0, 0, 0}
		case Size4:
			bits = [4]int{4, 0, 0, 0}
		default:
			return bits, fmt.Errorf("format I does not support size: %s", f.Size)
		}
	default:
		return bits, fmt.Errorf("cannot convert to format: %s", f)
	}
	return bits, nil
}
