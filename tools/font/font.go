package main

import (
	"bytes"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/draw"
	"image/png"
	"io/ioutil"
	"math"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"golang.org/x/text/encoding/charmap"
	"golang.org/x/text/encoding/htmlindex"

	"thornmarked/tools/font/charset"
	"thornmarked/tools/getpath"
	"thornmarked/tools/rectpack"
	"thornmarked/tools/texture"
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
	image   image.RGBA

	// From packing
	pos      [2]int32
	texindex int
}

type metrics struct {
	ascender  int32
	descender int32
	height    int32
}

type font struct {
	metrics  metrics
	charmap  map[uint32]uint32
	glyphs   []*glyph
	textures []*image.RGBA
}

func rasterizeFont(filename string, size int) (*font, error) {
	cmd := exec.Command(
		getpath.GetTool("tools/font/raster/raster"),
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
		case "metrics":
			if len(fields) != 3 {
				return nil, fmt.Errorf("metrics has %d fields, expect %d", len(fields), 3)
			}
			var val [3]int32
			for i, f := range fields {
				x, err := strconv.ParseInt(string(f), 10, 32)
				if err != nil {
					return nil, fmt.Errorf("invalid metric: %w", err)
				}
				val[i] = int32(x)
			}
			fn.metrics = metrics{
				ascender:  val[0],
				descender: val[1],
				height:    val[2],
			}
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
				gdata := make([]byte, g.size[0]*g.size[1])
				if _, err := hex.Decode(gdata, dfield); err != nil {
					return nil, fmt.Errorf("invalid data field: %v", err)
				}
				rdata := make([]byte, g.size[0]*g.size[1]*4)
				for i, y := range gdata {
					rdata[4*i] = y
					rdata[4*i+1] = y
					rdata[4*i+2] = y
					rdata[4*i+3] = y
				}
				g.image.Pix = rdata
				g.image.Stride = int(g.size[0]) * 4
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

func (fn *font) compact() {
	gset := make(map[uint32]bool)
	for _, g := range fn.charmap {
		gset[g] = true
	}
	gset[0] = true
	gmap := make([]uint32, len(fn.glyphs))
	var glyphs []*glyph
	for i, g := range fn.glyphs {
		if gset[uint32(i)] {
			gmap[i] = uint32(len(glyphs))
			glyphs = append(glyphs, g)
		}
	}
	charmap := make(map[uint32]uint32)
	for c, g := range fn.charmap {
		if g := gmap[g]; g != 0 {
			charmap[c] = g
		}
	}
	fn.charmap = charmap
	fn.glyphs = glyphs
}

func (fn *font) subset(charset map[uint32]bool) {
	for c := range fn.charmap {
		if !charset[c] {
			delete(fn.charmap, c)
		}
	}
}

func (fn *font) encode(cm *charmap.Charmap) {
	ncm := make(map[uint32]uint32)
	for c, g := range fn.charmap {
		if b, ok := cm.EncodeRune(rune(c)); ok {
			ncm[uint32(b)] = g
		}
	}
	fn.charmap = ncm
}

func (fn *font) addShadow(pos image.Point, alpha float64) error {
	// Alpha color for the shadow.
	scolor := math.Round(256 * alpha)
	var scolor8 uint8
	if scolor <= 0 {
		return nil
	} else if scolor >= 255 {
		scolor8 = 255
	} else {
		scolor8 = uint8(scolor)
	}
	simg := image.NewUniform(color.RGBA{A: scolor8})
	simg = image.NewUniform(color.RGBA{A: 255})

	// Alpha texture to hold the glyph. Must be as large as the largest glyph.
	var gsize [2]int32
	for _, g := range fn.glyphs {
		if g.size[0] > gsize[0] {
			gsize[0] = g.size[0]
		}
		if g.size[1] > gsize[1] {
			gsize[1] = g.size[1]
		}
	}
	ga := image.NewAlpha(image.Rectangle{
		Max: image.Point{
			X: int(gsize[0]),
			Y: int(gsize[1]),
		},
	})

	// Shadow color image.

	for _, g := range fn.glyphs {
		// Copy the glyph to the alpha texture. Alpha textures are faster to use
		// as masks.
		ss := g.image.Stride
		ds := ga.Stride
		xsz := g.image.Rect.Max.X - g.image.Rect.Min.X
		ysz := g.image.Rect.Max.Y - g.image.Rect.Min.Y
		for y := 0; y < ysz; y++ {
			srow := g.image.Pix[y*ss : y*ss+xsz*4 : y*ss+xsz*4]
			drow := ga.Pix[y*ds : y*ds+xsz : y*ds+xsz]
			for x := 0; x < xsz; x++ {
				drow[x] = srow[x*4+3]
			}
		}

		// Create a new glyph image, larger than the original.
		bounds := g.image.Rect
		if pos.X < 0 {
			bounds.Min.X += pos.X
		} else {
			bounds.Max.X += pos.X
		}
		if pos.Y < 0 {
			bounds.Max.Y += pos.Y
		} else {
			bounds.Max.Y += pos.Y
		}
		nimg := image.NewRGBA(bounds)

		// Render the drop shadow.
		rdest := g.image.Rect
		rdest.Min.X += pos.X
		rdest.Min.Y += pos.Y
		rdest.Max.X += pos.X
		rdest.Max.Y += pos.Y
		draw.DrawMask(nimg, rdest, simg, image.Point{}, ga, image.Point{}, draw.Src)

		// Draw the original glyph on top.
		draw.Draw(nimg, g.image.Rect, &g.image, g.image.Rect.Min, draw.Over)

		// Overwrite the glyph image.
		g.image = *nimg
		if pos.X > 0 {
			g.size[0] += int32(pos.X)
		} else {
			g.size[0] -= int32(pos.X)
			g.center[0] -= int32(pos.X)
		}
		if pos.Y > 0 {
			g.size[1] += int32(pos.Y)
		} else {
			g.size[1] -= int32(pos.Y)
			g.center[1] -= int32(pos.Y)
		}
	}
	return nil
}

func drawBox(im *image.RGBA, sz, x0, y0, x1, y1 int, c uint8) {
	for y := y0; y < y0+sz; y++ {
		for x := x0; x < x1; x++ {
			im.SetRGBA(x, y, color.RGBA{c, c, c, 0xff})
		}
	}
	for y := y1 - sz; y < y1; y++ {
		for x := x0; x < x1; x++ {
			im.SetRGBA(x, y, color.RGBA{c, c, c, 0xff})
		}
	}
	for x := x0; x < x0+sz; x++ {
		for y := y0 + sz; y < y1-sz; y++ {
			im.SetRGBA(x, y, color.RGBA{c, c, c, 0xff})
		}
	}
	for x := x1 - sz; x < x1; x++ {
		for y := y0 + sz; y < y1-sz; y++ {
			im.SetRGBA(x, y, color.RGBA{c, c, c, 0xff})
		}
	}
}

func (fn *font) makeGrid() *image.RGBA {
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
	im := image.NewRGBA(image.Rectangle{
		Max: image.Point{
			X: cols*cwidth + border,
			Y: rows*cheight + border,
		},
	})
	draw.Draw(im, im.Rect, image.NewUniform(color.RGBA{0, 0, 0, 0xff}), im.Rect.Min, draw.Src)
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
			draw.Over,
		)
	}
	return im
}

func (fn *font) pack(texsize image.Point) (*image.RGBA, error) {
	p := rectpack.New()
	sizes := make([]rectpack.Point, len(fn.glyphs))
	for i, g := range fn.glyphs {
		sizes[i] = rectpack.Point{X: g.size[0], Y: g.size[1]}
	}
	var bounds rectpack.Point
	var pos []rectpack.Point
	var err error
	if texsize.X != 0 {
		tsize := rectpack.Point{
			X: int32(texsize.X),
			Y: int32(texsize.Y),
		}
		count, ipos, err := rectpack.AutoPackMultiple(p, tsize, sizes)
		if err != nil {
			return nil, err
		}
		if count == 0 {
			return nil, errors.New("empty texture")
		}
		bounds = rectpack.Point{
			X: tsize.X,
			Y: tsize.Y * int32(count),
		}
		// Stack the bins vertically.
		pos = make([]rectpack.Point, len(ipos))
		for i, p := range ipos {
			pos[i] = rectpack.Point{
				X: p.Pos.X,
				Y: p.Pos.Y + tsize.Y*int32(p.Index),
			}
			fn.glyphs[i].pos = [2]int32{p.Pos.X, p.Pos.Y}
			fn.glyphs[i].texindex = p.Index
		}
		fn.textures = make([]*image.RGBA, count)
	} else {
		bounds, pos, err = rectpack.AutoPackSingle(p, sizes)
		if err != nil {
			return nil, err
		}
		if bounds.X == 0 {
			return nil, errors.New("empty texture")
		}
		for i, p := range pos {
			fn.glyphs[i].pos = [2]int32{p.X, p.Y}
		}
		fn.textures = make([]*image.RGBA, 1)
	}
	im := image.NewRGBA(image.Rectangle{
		Max: image.Point{
			X: int(bounds.X),
			Y: int(bounds.Y),
		},
	})
	for i, g := range fn.glyphs {
		px := int(pos[i].X)
		py := int(pos[i].Y)
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
	if texsize.X != 0 {
		for i := range fn.textures {
			fn.textures[i] = im.SubImage(image.Rectangle{
				Min: image.Point{
					Y: i * int(texsize.Y),
				},
				Max: image.Point{
					X: int(texsize.X),
					Y: (i + 1) * int(texsize.Y),
				},
			}).(*image.RGBA)
		}
	} else {
		fn.textures[0] = im
	}
	return im, nil
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

type options struct {
	font         string
	size         int
	grid         string
	texfile      string
	datafile     string
	texsize      image.Point
	texfmt       texture.SizedFormat
	charset      charset.Set
	encoding     *charmap.Charmap
	removeNotdef bool
	mono         bool
	fallbackfile string
	shadow       bool
	shadowPos    image.Point
	shadowAlpha  float64
}

func parseSize(s string) (pt image.Point, err error) {
	i := strings.IndexByte(s, ':')
	if i == -1 {
		return pt, errors.New("missing ':'")
	}
	x, err := strconv.ParseUint(s[:i], 10, strconv.IntSize-1)
	if err != nil {
		return pt, fmt.Errorf("invalid width: %w", err)
	}
	y, err := strconv.ParseUint(s[i+1:], 10, strconv.IntSize-1)
	if err != nil {
		return pt, fmt.Errorf("invalid height: %w", err)
	}
	pt.X = int(x)
	pt.Y = int(y)
	return pt, nil
}

func parseCP(s string) (uint32, error) {
	n, err := strconv.ParseUint(s, 16, 32)
	if err != nil {
		return 0, fmt.Errorf("invalid code point %q: %v", s, err)
	}
	return uint32(n), nil
}

func parseShadow(arg string, o *options) error {
	fields := strings.Split(arg, ":")
	if len(fields) < 2 {
		return fmt.Errorf("got %d fields, expected at least 2", len(fields))
	}
	x, err := strconv.ParseInt(fields[0], 10, strconv.IntSize)
	if err != nil {
		return fmt.Errorf("invalid x %q: %v", fields[0], err)
	}
	y, err := strconv.ParseInt(fields[1], 10, strconv.IntSize)
	if err != nil {
		return fmt.Errorf("invalid y %q: %v", fields[1], err)
	}
	var alpha float64 = 1.0
	if len(fields) >= 3 {
		alpha, err = strconv.ParseFloat(fields[2], 32)
		if err != nil {
			return fmt.Errorf("invalid alpha %q: %v", fields[2], err)
		}
		if !(0.0 <= alpha && alpha <= 1.0) {
			return fmt.Errorf("alpha not between 0 and 1: %q", fields[2])
		}
	}
	o.shadow = true
	o.shadowPos = image.Point{X: int(x), Y: int(y)}
	o.shadowAlpha = alpha
	return nil
}

func parseOpts() (o options, err error) {
	fontArg := flag.String("font", "", "font to rasterize")
	sizeArg := flag.Int("size", 0, "size to rasterize font at")
	gridArg := flag.String("out-grid", "", "grid preview output file")
	textureArg := flag.String("out-texture", "", "output texture")
	texsizeArg := flag.String("texture-size", "", "pack into multiple regions of size `WIDTH:HEIGHT`")
	charsetArg := flag.String("charset", "", "path to character set file")
	encodingArg := flag.String("encoding", "", "name of character encoding to use")
	removeNotdefArg := flag.Bool("remove-notdef", false, "remove the .notdef glyph")
	outDataArg := flag.String("out-data", "", "output font data file")
	flag.Var(&o.texfmt, "format", "use `format.size` texture format")
	flag.BoolVar(&o.mono, "mono", false, "render monochrome (1-bit, instead of grayscale)")
	outFallbackArg := flag.String("out-fallback", "", "output for fallback font data")
	shadowArg := flag.String("shadow", "", "add drop shadow `x:y[:alpha]`")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return o, fmt.Errorf("unexpected argument: %q", args[0])
	}
	o.font = getpath.GetPath(*fontArg)
	if o.font == "" {
		return o, errors.New("missing required flag -font")
	}
	o.size = *sizeArg
	if o.size == 0 {
		return o, errors.New("missing required flag -size")
	}
	if o.size < minSize || maxSize < o.size {
		return o, fmt.Errorf("invalid size %d, must be between %d and %d", o.size, minSize, maxSize)
	}
	o.grid = getpath.GetPath(*gridArg)
	o.texfile = getpath.GetPath(*textureArg)
	o.datafile = getpath.GetPath(*outDataArg)
	if o.datafile != "" && o.texfmt.Format == texture.UnknownFormat {
		return o, errors.New("the -format flag must be used when using -out-data")
	}
	if sz := *texsizeArg; sz != "" {
		if o.texfile == "" && o.datafile == "" {
			return o, fmt.Errorf("cannot use -texture-size without -out-texture or -out-data")
		}
		pt, err := parseSize(sz)
		if err != nil {
			return o, fmt.Errorf("invalid size: %w", err)
		}
		if pt.X == 0 || pt.Y == 0 {
			return o, errors.New("zero size")
		}
		o.texsize = pt
	}
	if *charsetArg != "" {
		cs, err := charset.ReadFile(getpath.GetPath(*charsetArg))
		if err != nil {
			return o, err
		}
		o.charset = cs
	}
	if *encodingArg != "" {
		enc, err := htmlindex.Get(*encodingArg)
		if err != nil {
			return o, fmt.Errorf("unknown encoding %q: %v", *encodingArg, err)
		}
		cm, ok := enc.(*charmap.Charmap)
		if !ok {
			return o, fmt.Errorf("encoding is not a simple charmap: %q", *encodingArg)
		}
		o.encoding = cm
	}
	o.removeNotdef = *removeNotdefArg
	o.fallbackfile = getpath.GetPath(*outFallbackArg)
	if *shadowArg != "" {
		if err := parseShadow(*shadowArg, &o); err != nil {
			return o, fmt.Errorf("invalid -shadow %q: %v", *shadowArg, err)
		}
	}

	return o, nil
}

func mainE() error {
	opts, err := parseOpts()
	if err != nil {
		return err
	}
	fn, err := rasterizeFont(opts.font, opts.size)
	if err != nil {
		return err
	}
	if opts.charset != nil {
		fn.subset(opts.charset)
	}
	if opts.encoding != nil {
		fn.encode(opts.encoding)
	}
	fn.compact()
	if opts.removeNotdef {
		fn.glyphs[0].image = image.RGBA{}
	}
	if opts.shadow {
		if err := fn.addShadow(opts.shadowPos, opts.shadowAlpha); err != nil {
			return fmt.Errorf("could not add shadow: %v", err)
		}
	}
	if opts.texfmt.Format != texture.UnknownFormat {
		for _, g := range fn.glyphs {
			if err := texture.ToSizedFormat(opts.texfmt, &g.image, texture.NoDither); err != nil {
				return err
			}
		}
	}
	if opts.grid != "" {
		im := fn.makeGrid()
		if err := writeImage(im, opts.grid); err != nil {
			return err
		}
	}
	if opts.fallbackfile != "" {
		data, err := fn.makeFallback()
		if err != nil {
			return err
		}
		if err := ioutil.WriteFile(opts.fallbackfile, data, 066); err != nil {
			return err
		}
	}
	if opts.texfile != "" || opts.datafile != "" {
		im, err := fn.pack(opts.texsize)
		if err != nil {
			return fmt.Errorf("could not pack: %w", err)
		}
		if opts.texfile != "" {
			if err := writeImage(im, opts.texfile); err != nil {
				return err
			}
		}
		if opts.datafile != "" {
			data, err := makeAsset(fn, opts.texfmt)
			if err != nil {
				return err
			}
			if err := ioutil.WriteFile(opts.datafile, data, 0666); err != nil {
				return err
			}
		}
	}
	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
