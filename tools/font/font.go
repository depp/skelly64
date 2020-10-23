package main

import (
	"bytes"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
)

const (
	minSize = 4
	maxSize = 16 * 1024
)

type glyph struct {
	size    [2]int32
	center  [2]int32
	advance int32
	name    string
	image   image.Gray
}

type font struct {
	charmap map[uint32]uint32
	glyphs  []*glyph
}

func rasterizeFont(filename string, size int) (*font, error) {
	cmd := exec.Command(
		"tools/font/raster",
		"rasterize",
		"-font="+filename,
		"-size="+strconv.Itoa(size),
	)
	var buf bytes.Buffer
	cmd.Stdout = &buf
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return nil, err
	}
	data := buf.Bytes()
	fn := font{
		charmap: make(map[uint32]uint32),
	}
	for len(data) != 0 {
		i := bytes.IndexByte(data, '\n')
		var line []byte
		if i == -1 {
			line = data
			data = nil
		} else {
			line = data[:i]
			data = data[i+1:]
		}
		fields := bytes.Split(line, []byte(" "))
		if len(fields) == 0 {
			return nil, fmt.Errorf("invalid line: %q", line)
		}
		rtype := fields[0]
		fields = fields[1:]
		switch string(rtype) {
		case "char":
			if len(fields) != 2 {
				return nil, fmt.Errorf("char has %d fields, expect %d", len(fields), 2)
			}
			char, err := strconv.ParseUint(string(fields[0]), 10, 32)
			if err != nil {
				return nil, fmt.Errorf("invalid character %q: %v", fields[0], err)
			}
			gl, err := strconv.ParseUint(string(fields[1]), 10, 32)
			if err != nil {
				return nil, fmt.Errorf("invalid glyph %q: %v", fields[1], err)
			}
			fn.charmap[uint32(char)] = uint32(gl)
		case "glyph":
			if len(fields) != 7 {
				return nil, fmt.Errorf("glyph has %d fields, expect %d", len(fields), 7)
			}
			var nums [5]int32
			for i, f := range fields[:5] {
				x, err := strconv.ParseInt(string(f), 10, 32)
				if err != nil {
					return nil, fmt.Errorf("invalid glyph field %q: %v", f, err)
				}
				nums[i] = int32(x)
			}
			g := glyph{
				size:    [2]int32{nums[0], nums[1]},
				center:  [2]int32{nums[2], nums[3]},
				advance: nums[4],
				name:    string(fields[5]),
			}
			if g.size[0] > 0 && g.size[1] > 0 {
				if g.size[0] > maxSize || g.size[1] > maxSize {
					return nil, fmt.Errorf("glyph size too large: %dx%d", g.size[0], g.size[1])
				}
				size := int(g.size[0]) * int(g.size[1])
				dfield := fields[6]
				if len(dfield) != size*2 {
					return nil, fmt.Errorf("glyph data is %d characters, expected %d",
						len(dfield), size*2)
				}
				data := make([]byte, g.size[0]*g.size[1])
				if _, err := hex.Decode(data, dfield); err != nil {
					return nil, fmt.Errorf("invalid data field: %v", err)
				}
				g.image.Pix = data
				g.image.Stride = int(g.size[0])
				g.image.Rect.Max = image.Point{
					X: int(g.size[0]),
					Y: int(g.size[1]),
				}
			}
			fn.glyphs = append(fn.glyphs, &g)
		default:
			return nil, fmt.Errorf("unknown record: %q", rtype)
		}
	}
	return &fn, nil
}

func drawBox(im *image.Gray, sz, x0, y0, x1, y1 int, c uint8) {
	for y := y0; y < y0+sz; y++ {
		for x := x0; x < x1; x++ {
			im.Pix[im.PixOffset(x, y)] = c
		}
	}
	for y := y1 - sz; y < y1; y++ {
		for x := x0; x < x1; x++ {
			im.Pix[im.PixOffset(x, y)] = c
		}
	}
	for x := x0; x < x0+sz; x++ {
		for y := y0 + sz; y < y1-sz; y++ {
			im.Pix[im.PixOffset(x, y)] = c
		}
	}
	for x := x1 - sz; x < x1; x++ {
		for y := y0 + sz; y < y1-sz; y++ {
			im.Pix[im.PixOffset(x, y)] = c
		}
	}
}

func (fn *font) makeGrid() *image.Gray {
	const (
		border      = 1
		margin      = 2
		space       = border + margin*2
		borderColor = 0x80
	)
	cols := 16
	if cols > len(fn.glyphs) {
		cols = len(fn.glyphs)
	}
	rows := (len(fn.glyphs) + cols - 1) / cols
	var cwidth, cheight int
	for _, g := range fn.glyphs {
		if w := int(g.size[0]); w > cwidth {
			cwidth = w
		}
		if h := int(g.size[1]); h > cheight {
			cheight = h
		}
	}
	cwidth += space
	cheight += space
	im := image.NewGray(image.Rectangle{
		Max: image.Point{
			X: cols*cwidth + border,
			Y: rows*cheight + border,
		},
	})
	for i, g := range fn.glyphs {
		cx := i % cols
		cy := i / cols
		px := cwidth * cx
		py := cheight * cy
		drawBox(im, border, px, py, px+cwidth+border, py+cheight+border, borderColor)
		px += border + margin
		py += border + margin
		draw.Draw(
			im,
			image.Rectangle{
				Min: image.Point{X: px, Y: py},
				Max: image.Point{X: px + int(g.size[0]), Y: py + int(g.size[1])},
			},
			&g.image,
			image.Point{},
			draw.Src,
		)
	}
	return im
}

func getPath(wd, filename string) string {
	if filename == "" {
		return ""
	}
	if !filepath.IsAbs(filename) && wd != "" {
		return filepath.Join(wd, filename)
	}
	return filename
}

func writeImage(im image.Image, dest string) error {
	fp, err := os.Create(dest)
	if err != nil {
		return err
	}
	defer fp.Close()
	if err := png.Encode(fp, im); err != nil {
		return err
	}
	return fp.Close()
}

func mainE() error {
	wd := os.Getenv("BUILD_WORKING_DIRECTORY")
	fontArg := flag.String("font", "", "font to rasterize")
	sizeArg := flag.Int("size", 0, "size to rasterize font at")
	gridArg := flag.String("out-grid", "", "grid preview output file")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	fontfile := getPath(wd, *fontArg)
	if fontfile == "" {
		return errors.New("missing required flag -font")
	}
	gridfile := getPath(wd, *gridArg)

	size := *sizeArg
	if size == 0 {
		return errors.New("missing required flag -size")
	}
	if size < minSize || maxSize < size {
		return fmt.Errorf("invalid size %d, must be between %d and %d", size, minSize, maxSize)
	}

	fn, err := rasterizeFont(fontfile, size)
	if err != nil {
		return err
	}
	if gridfile != "" {
		im := fn.makeGrid()
		if err := writeImage(im, gridfile); err != nil {
			return err
		}
	}
	fmt.Println("OK")
	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
