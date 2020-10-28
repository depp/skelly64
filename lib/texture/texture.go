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
	"strings"
)

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

func rga16from32(data []byte) []byte {
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

	inimg, err := readPNG(input)
	if err != nil {
		return err
	}
	data := rga16from32(packRGBA32(toRGBA(inimg)))
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
