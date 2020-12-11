package main

import (
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"image"
	"image/png"
	"io/ioutil"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"thornmarked/tools/getpath"
	"thornmarked/tools/texture"
)

const tmemSize = 4096

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

type options struct {
	output    string
	input     string
	format    texture.SizedFormat
	layout    texture.Layout
	mipmap    bool
	strips    bool
	dithering texture.Dithering
	anchor    [2]float64
	gamma     float64
}

func parseArgs() (opts options, err error) {
	output := flag.String("output", "", "write output to `file`")
	input := flag.String("input", "", "read input texture")
	native := flag.Bool("native", false, "use native TMEM texture layout")
	flag.BoolVar(&opts.mipmap, "mipmap", false, "generate mipmaps")
	flag.BoolVar(&opts.strips, "strips", false, "convert to strips that fit in TMEM")
	flag.Var(&opts.format, "format", "use texture format `fmt.size` (e.g. rgba.16)")
	dither := flag.String("dither", "", "use dithering algorithm (none, bayer, floyd-steinberg)")
	anchor := flag.String("anchor", "", "origin of image (strips only), `x:y` range 0-1")
	flag.Float64Var(&opts.gamma, "gamma", 0, "use `N` as the source image gamma")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return opts, fmt.Errorf("unexpected argument: %q", args[0])
	}
	opts.output = getpath.GetPath(*output)
	if opts.output == "" {
		return opts, errors.New("missing required flag -output")
	}
	opts.input = getpath.GetPath(*input)
	if opts.input == "" {
		return opts, errors.New("missing required flag -input")
	}
	if *native {
		opts.layout = texture.Native
	} else {
		opts.layout = texture.Linear
	}
	if *dither == "" {
		opts.dithering = texture.FloydSteinberg
	} else {
		opts.dithering, err = texture.ParseDithering(*dither)
		if err != nil {
			return opts, fmt.Errorf("invalid value for -dither: %v", err)
		}
	}
	if *anchor != "" {
		fields := strings.Split(*anchor, ":")
		if len(fields) != 2 {
			return opts, fmt.Errorf("invalid anchor, expected two numbers separated by ':': %q", *anchor)
		}
		x, err := strconv.ParseFloat(fields[0], 64)
		if err != nil {
			return opts, fmt.Errorf("invalid anchor: %v", err)
		}
		y, err := strconv.ParseFloat(fields[1], 64)
		if err != nil {
			return opts, fmt.Errorf("invalid anchor: %v", err)
		}
		opts.anchor = [2]float64{x, y}
	} else {
		opts.anchor = [2]float64{0.5, 0.5}
	}
	return opts, nil
}

func makeTexture(opts *options, img *image.RGBA) ([]byte, error) {
	const (
		dlNone = iota
		dlRGBA16s32x32
		dlI4s64x63
		dlCI4s32x32
	)
	maxSize := tmemSize
	if opts.format.Format == texture.CI {
		maxSize >>= 1
	}

	// Create image tiles at sizes that fit in TMEM.
	i16 := texture.ToRGBA16(img, opts.gamma)
	i16, err := texture.AutoScale(i16, maxSize, opts.format.Size.Size(), opts.mipmap)
	if err != nil {
		return nil, fmt.Errorf("could not scale texture: %v", err)
	}
	var tiles []*image.RGBA64
	if opts.mipmap {
		tiles, err = texture.CreateMipMaps(i16)
		if err != nil {
			return nil, fmt.Errorf("could not create mipmaps: %v", err)
		}
	} else {
		tiles = []*image.RGBA64{i16}
	}
	tsize := i16.Rect.Size()
	var dl int
	f := opts.format
	if tsize.X == 32 && tsize.Y == 32 {
		if f.Format == texture.RGBA && f.Size == texture.Size16 {
			dl = dlRGBA16s32x32
		} else if f.Format == texture.CI && f.Size == texture.Size4 {
			dl = dlCI4s32x32
		}
	} else if tsize.X == 64 && tsize.Y == 64 {
		if f.Format == texture.I && f.Size == texture.Size4 {
			dl = dlI4s64x63
		}
	}
	if dl == 0 {
		return nil, errors.New("texture does not match a known display list")
	}

	// Convert texture to desired format and pack into contiguous block.
	data := make([]byte, 24)
	copy(data, "Texture")
	binary.BigEndian.PutUint32(data[16:], uint32(dl))
	for _, tile := range tiles {
		img := texture.ToRGBA8(tile)
		if err := texture.ToSizedFormat(opts.format, img, opts.dithering); err != nil {
			return nil, fmt.Errorf("could not convert to %s: %v", opts.format, err)
		}
		tdata, err := texture.Pack(img, opts.format, opts.layout)
		if err != nil {
			return nil, fmt.Errorf("could not pack texture: %v", err)
		}
		data = append(data, tdata...)
	}

	return data, nil
}

func mainE() error {
	opts, err := parseArgs()
	if err != nil {
		return err
	}

	// Load image as RGBA with 8 bits per sample.
	inimg, err := readPNG(opts.input)
	if err != nil {
		return err
	}
	img := texture.ToRGBA(inimg)

	var data []byte
	switch {
	case opts.strips:
		data, err = makeStrips(&opts, img)
	default:
		data, err = makeTexture(&opts, img)
	}
	if err != nil {
		return err
	}

	if err := ioutil.WriteFile(opts.output, data, 0666); err != nil {
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
