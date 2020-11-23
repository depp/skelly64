package main

import (
	"errors"
	"flag"
	"fmt"
	"image"
	"image/png"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

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

func mainE() error {
	outFlag := flag.String("output", "", "write output to `file`")
	var tfmt texture.SizedFormat
	flag.Var(&tfmt, "format", "use texture format `fmt.size` (e.g. rgba.16)")
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
	if tfmt.Format == 0 {
		return errors.New("missing required -format flag")
	}
	input := args[0]

	inimg, err := readPNG(input)
	if err != nil {
		return err
	}
	img := texture.ToRGBA(inimg)
	{
		i16 := texture.ToRGBA16(img)
		i16, err := texture.AutoScale(i16, tmemSize, tfmt.Size.Size())
		if err != nil {
			return err
		}
		img = texture.ToRGBA8(i16)
	}
	if err := texture.ToSizedFormat(tfmt, img); err != nil {
		return err
	}
	fdata := make([]byte, 16)
	copy(fdata, "Texture")
	data, err := texture.Pack(tfmt, img)
	if err != nil {
		return err
	}
	fdata = append(fdata, data...)
	if err := ioutil.WriteFile(output, fdata, 0666); err != nil {
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
