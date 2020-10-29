package main

import (
	"errors"
	"flag"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"io/ioutil"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

type iformat uint32

const (
	fUnknown iformat = iota
	fRGBA
	fCI
	fIA
	fI
)

var fnames = [...]string{
	fRGBA: "RGBA",
	fCI:   "CI",
	fIA:   "IA",
	fI:    "I",
}

func (v iformat) String() (s string) {
	i := uint32(v)
	if i < uint32(len(fnames)) {
		s = fnames[i]
	}
	if s == "" {
		s = strconv.FormatUint(uint64(i), 10)
	}
	return
}

func (v *iformat) Set(s string) error {
	for i, n := range fnames {
		if strings.EqualFold(s, n) {
			*v = iformat(i)
			return nil
		}
	}
	return fmt.Errorf("unknown format: %q", s)
}

type isize uint32

const (
	szUnknown isize = iota
	sz32
	sz16
	sz8
	sz4
)

var isizes = [...]uint32{
	sz32: 32,
	sz16: 16,
	sz8:  8,
	sz4:  4,
}

func (v isize) String() (s string) {
	i := uint32(v)
	if i < uint32(len(isizes)) {
		sz := isizes[i]
		if sz != 0 {
			return strconv.FormatUint(uint64(sz), 10)
		}
	}
	return "isize(" + strconv.FormatUint(uint64(i), 10) + ")"
}

func (v *isize) Set(s string) error {
	n, err := strconv.ParseUint(s, 10, 32)
	if err != nil {
		return err
	}
	n32 := uint32(n)
	for i, x := range isizes {
		if x == n32 && n32 != 0 {
			*v = isize(i)
			return nil
		}
	}
	return fmt.Errorf("unsupported size: %d", n)
}

func readPNG(filename string) (image.Image, error) {
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

func toRGBA(im image.Image) *image.RGBA {
	if ri, ok := im.(*image.RGBA); ok {
		return ri
	}
	b := im.Bounds()
	ri := image.NewRGBA(b)
	draw.Draw(ri, b, im, b.Min, draw.Src)
	return ri
}

func packRGBA32(im *image.RGBA) []byte {
	xsz := im.Rect.Max.X - im.Rect.Min.X
	ysz := im.Rect.Max.Y - im.Rect.Min.Y
	orow := xsz * 4
	out := make([]byte, ysz*orow)
	for y := 0; y < ysz; y++ {
		copy(out[y*orow:(y+1)*orow], im.Pix[y*im.Stride:])
	}
	return out
}

func rgba16from32(data []byte) []byte {
	out := make([]byte, len(data)/2)
	for i := 0; i < len(data)/4; i++ {
		pix := data[i*4 : i*4+4 : i*4+4]
		r := pix[0]
		g := pix[1]
		b := pix[2]
		color := (uint32(r)<<8)&0xf100 |
			(uint32(g)<<3)&0x07c0 |
			(uint32(b)>>2)&0x003e
		out[2*i] = byte(color >> 8)
		out[2*i+1] = byte(color)
	}
	return out
}

func mainE() error {
	outFlag := flag.String("output", "", "write output to `file`")
	ifmt := fRGBA
	flag.Var(&ifmt, "format", "use image format `fmt` (rgba,ci,ia,i)")
	isz := sz16
	flag.Var(&isz, "size", "use pixel size `size` (32,16,8,4)")
	flag.Parse()
	args := flag.Args()
	if len(args) == 0 {
		return errors.New("missing image argument")
	}
	if len(args) != 1 {
		return fmt.Errorf("got %d args, expected exactly 1", len(args))
	}
	output := *outFlag
	if output == "" {
		return errors.New("missing required -output flag")
	}
	input := args[0]

	fmt.Println("Format:", ifmt, isz)

	inimg, err := readPNG(input)
	if err != nil {
		return err
	}
	var data []byte
	switch ifmt {
	case fRGBA:
		ri := toRGBA(inimg)
		switch isz {
		case sz32:
			data = packRGBA32(ri)
		case sz16:
			data = rgba16from32(packRGBA32(ri))
		default:
			return fmt.Errorf("size %s is not supported for RGBA images", isz)
		}
	default:
		return fmt.Errorf("image format unimplemented: %s", ifmt)
	}
	if err := ioutil.WriteFile(output, data, 0666); err != nil {
		return err
	}
	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
