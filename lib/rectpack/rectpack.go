// Package rectpack packs rectangles into larger rectangles.
package rectpack

import (
	"errors"
	"math"
)

// A Point is a 2D point.
type Point struct {
	X int32
	Y int32
}

func (p Point) roundUpPow2() Point {
	return Point{
		X: roundUpPow2(p.X),
		Y: roundUpPow2(p.Y),
	}
}

func (p Point) betterThan(o Point) bool {
	pa := p.X * p.Y
	oa := o.X * o.Y
	if pa < oa {
		return true
	}
	if pa > oa {
		return false
	}
	pd := p.X - p.Y
	if pd < 0 {
		pd = -pd
	}
	od := o.X - o.Y
	if od < 0 {
		od = -od
	}
	if pd < od {
		return true
	}
	if pd > od {
		return false
	}
	return p.X > o.X
}

// A rect is a rectangle.
type rect struct {
	min Point
	max Point
}

// An inrect is an input rectangle.
type inrect struct {
	index int
	size  Point
	pos   Point
}

// A rectSet is a list of rectangles to pack. It contains both packed and
// unpacked rectangles.
type rectSet struct {
	unpacked []inrect
	packed   []inrect
}

// Intersect returns true if the rects intersect.
func (r rect) intersect(o rect) bool {
	return r.max.X > o.min.X && r.min.X < o.max.X && r.max.Y > o.min.Y && r.min.Y < o.max.Y
}

// Contains returns true if this rect contains the given rect.
func (r rect) contains(o rect) bool {
	return r.min.X <= o.min.X && r.max.X >= o.max.X && r.min.Y <= o.min.Y && r.max.Y >= o.max.Y
}

// A Packer is an algorithm which packs rectangles into larger bounds.
type Packer interface {
	// Name returns the name of the algorithm.
	Name() string

	// Reset resets the packer to contain free space with the given bounds.
	Reset(bounds Point)

	// AddRect adds a rectangle to the packing and returns the minimum
	// coordinate of the rectangle. Returns false if no space can be found.
	AddRect(size Point) (pos Point, ok bool)

	// addRects adds rectangles from the rectset to the packing. This modifies
	// the set.
	addRects(rs *rectSet)

	// sort sorts the rectangles according to the packer's sort order.
	sort(rects []inrect)

	// isYOrdererd returns true if the packer always tries to pack in ascending
	// Y coordinates.
	isYOrdered() bool
}

func makeRects(p Packer, size []Point) (rects []inrect, minw, minh, area int32) {
	rects = make([]inrect, 0, len(size))
	for index, sz := range size {
		if sz.X > 0 && sz.Y > 0 {
			rects = append(rects, inrect{index: index, size: sz})
			if sz.X > minw {
				minw = sz.X
			}
			if sz.Y > minh {
				minh = sz.Y
			}
			area += sz.X * sz.Y
		}
	}
	rects = rects[0:len(rects):len(rects)]
	p.sort(rects)
	return
}

func rectBounds(rects []inrect) (b Point) {
	for _, r := range rects {
		if x := r.pos.X + r.size.X; x > b.X {
			b.X = x
		}
		if y := r.pos.Y + r.size.Y; y > b.Y {
			b.Y = y
		}
	}
	return
}

func finishRects(size []Point, rects []inrect) (pos []Point) {
	pos = make([]Point, len(size))
	for i, r := range rects {
		pos[i] = r.pos
	}
	return
}

func roundUpPow2(x int32) int32 {
	x--
	x |= x >> 1
	x |= x >> 2
	x |= x >> 4
	x |= x >> 8
	x |= x >> 16
	return x + 1
}

var errPackFailed = errors.New("could not pack rectangles")

// AutoPackSingle packs the given rectangles into a single bin, making the bin
// as large as necessary to contain all rectangles. Tries to minimize wasted
// space when the bounds are increased to powers of two. Returns the size of the
// bin and the location of each rectangle.
func AutoPackSingle(p Packer, size []Point) (bounds Point, pos []Point, err error) {
	rects, minw, minh, area := makeRects(p, size)
	if len(rects) == 0 {
		return Point{}, finishRects(size, nil), nil
	}
	rectarr := make([]inrect, len(rects))
	if p.isYOrdered() {
		// For algorithms that pack in Y order, we can try different widths and
		// the algorithm will calculate the height for us.
		width := roundUpPow2(minw)
		for width*width*4 <= area {
			width *= 2
		}
		var bRects []inrect
		var bBounds, bBounds2 Point
		for i := 0; i < 5; i++ {
			p.Reset(Point{X: width << i, Y: math.MaxInt32})
			copy(rectarr, rects)
			rs := rectSet{unpacked: rectarr}
			p.addRects(&rs)
			if len(rs.unpacked) == 0 {
				pRects := rs.packed
				pBounds := rectBounds(pRects)
				pBounds2 := pBounds.roundUpPow2()
				if bRects == nil || pBounds2.betterThan(bBounds2) {
					bRects = pRects
					bBounds = pBounds
					bBounds2 = pBounds2
				}
				if pBounds2.X >= pBounds2.Y {
					break
				}
			}
		}
		if bRects == nil {
			return Point{}, nil, errPackFailed
		}
		return bBounds, finishRects(size, bRects), nil
	}
	bsize := minw
	if minh > bsize {
		bsize = minh
	}
	for bsize*bsize*2 < area {
		bsize *= 2
	}
	// We try increasing the size until one fits. We try in order of preference,
	// so we just return the first one that fits.
	for i := 0; i < 5; i++ {
		s := bsize << i
		bvalues := [3]Point{
			{X: s, Y: s},
			{X: s * 2, Y: s},
			{X: s, Y: s * 2},
		}
		for _, bounds := range bvalues {
			p.Reset(bounds)
			copy(rectarr, rects)
			rs := rectSet{unpacked: rectarr}
			p.addRects(&rs)
			if len(rs.unpacked) == 0 {
				result := rs.packed
				return rectBounds(result), finishRects(size, result), nil
			}
		}
	}
	return Point{}, nil, errPackFailed
}

// An IndexPoint is a point in one of multiple possible bins. The index
// identifies which bin it is in.
type IndexPoint struct {
	Index int
	Pos   Point
}

// AutoPackMultiple packes the given rectangels into multiple bins of the same
// size, using as many bins as necessary to contain all the rectangles. Returns
// the number of bins and the locations of the rectangles.
func AutoPackMultiple(p Packer, bounds Point, size []Point) (count int, pos []IndexPoint, err error) {
	rects, minw, minh, _ := makeRects(p, size)
	if minw > bounds.X || minh > bounds.Y {
		return 0, nil, errPackFailed
	}
	pos = make([]IndexPoint, len(size))
	rs := rectSet{unpacked: rects}
	for len(rs.unpacked) > 0 {
		rs.packed = rs.packed[:0]
		p.Reset(bounds)
		p.addRects(&rs)
		if len(rs.packed) == 0 {
			panic("no packing")
		}
		for _, r := range rs.packed {
			pos[r.index] = IndexPoint{Index: count, Pos: r.pos}
		}
		count++
	}
	return
}

func defaultAddRects(p Packer, rs *rectSet) {
	var rpos int
	for _, r := range rs.unpacked {
		if pos, ok := p.AddRect(r.size); ok {
			rs.packed = append(rs.packed, inrect{
				index: r.index,
				size:  r.size,
				pos:   pos,
			})
		} else {
			rs.unpacked[rpos] = r
			rpos++
		}
	}
	rs.unpacked = rs.unpacked[:rpos]
}

// AllAlgorithms returns implementations of all supported algorithms.
func AllAlgorithms() []Packer {
	packers := []Packer{
		new(MaxRectsBL),
	}
	var r []Packer
	for _, p := range packers {
		r = append(r, p)
		for ord := minOrder; ord <= maxOrder; ord++ {
			r = append(r, &SortedPacker{
				Packer: p,
				Order:  ord,
			})
		}
	}
	return r
}

// New returns the default packer algorithm.
func New() Packer {
	return &SortedPacker{
		Packer: new(MaxRectsBL),
		Order:  HeightDesc,
	}
}
