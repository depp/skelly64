package rectpack

import (
	"sort"
	"strconv"
)

// An Order is a rectangle sort order.
type Order uint32

const (
	// Unsorted does not sort rectangles.
	Unsorted Order = iota
	// WidthDesc sorts widest rectangle first.
	WidthDesc
	// WidthAsc sorts widest rectangle last.
	WidthAsc
	// HeightDesc sorts tallest rectangle first.
	HeightDesc
	// HeightAsc sorts tallest rectangle last.
	HeightAsc
	// AreaDesc sorts largest area rectangle first.
	AreaDesc
	// AreaAsc sorts largest area rectangle last.
	AreaAsc
	// PerimeterDesc sorts largest perimeter first.
	PerimeterDesc
	// PerimeterAsc sorts largest perimeter last.
	PerimeterAsc
	// DifferenceDesc sorts largest difference between width and height first.
	DifferenceDesc
	// DifferenceAsc sorts largest difference between width and height last.
	DifferenceAsc
	// RatioDesc sorts largest aspect ratio first.
	RatioDesc
	// RatioAsc sorts largest aspect ratio last.
	RatioAsc
	// Lowest and highest sort order.
	minOrder = WidthDesc
	maxOrder = RatioAsc
)

var names = [...]string{
	Unsorted:       "Unsorted",
	WidthDesc:      "WidthDesc",
	WidthAsc:       "WidthAsc",
	HeightDesc:     "HeightDesc",
	HeightAsc:      "HeightAsc",
	AreaDesc:       "AreaDesc",
	AreaAsc:        "AreaAsc",
	PerimeterDesc:  "PerimeterDesc",
	PerimeterAsc:   "PerimeterAsc",
	DifferenceDesc: "DifferenceDesc",
	DifferenceAsc:  "DifferenceAsc",
	RatioDesc:      "RatioDesc",
	RatioAsc:       "RatioAsc",
}

// String implements the Stringer interface.
func (o Order) String() (s string) {
	i := uint32(o)
	if i < uint32(len(names)) {
		s = names[i]
	}
	if s == "" {
		s = strconv.FormatUint(uint64(i), 10)
	}
	return
}

func ordWidth(a, b Point, asc, ord bool) bool {
	switch {
	case a.X < b.X:
		return asc
	case a.X > b.X:
		return !asc
	case a.Y < b.Y:
		return asc
	case a.Y > b.Y:
		return !asc
	default:
		return ord
	}
}

func ordHeight(a, b Point, asc, ord bool) bool {
	switch {
	case a.Y < b.Y:
		return asc
	case a.Y > b.Y:
		return !asc
	case a.X < b.X:
		return asc
	case a.X > b.X:
		return !asc
	default:
		return ord
	}
}

func ordArea(a, b Point, asc, ord bool) bool {
	va := a.X * a.Y
	vb := b.X * b.Y
	switch {
	case va < vb:
		return asc
	case vb > va:
		return asc
	default:
		return ord
	}
}

func ordPerimeter(a, b Point, asc, ord bool) bool {
	va := a.X + a.Y
	vb := b.X + b.Y
	switch {
	case va < vb:
		return asc
	case vb > va:
		return asc
	default:
		return ord
	}
}

func ordDifference(a, b Point, asc, ord bool) bool {
	va := a.X - a.Y
	vb := b.X - b.Y
	if va < 0 {
		va = -va
	}
	if vb < 0 {
		vb = -vb
	}
	switch {
	case va < vb:
		return asc
	case vb > va:
		return asc
	default:
		return ord
	}
}

func ordRatio(a, b Point, asc, ord bool) bool {
	va := a.X * b.Y
	vb := b.X * a.Y
	switch {
	case va < vb:
		return asc
	case vb > va:
		return asc
	default:
		return ord
	}
}

type rslice struct {
	rects []inrect
	order Order
}

func (r rslice) Len() int { return len(r.rects) }

func (r rslice) Less(i, j int) bool {
	iorder := r.rects[i].index < r.rects[j].index
	sx := r.rects[i].size
	sy := r.rects[j].size
	switch r.order {
	case WidthDesc:
		return ordWidth(sx, sy, false, iorder)
	case WidthAsc:
		return ordWidth(sx, sy, true, iorder)
	case HeightDesc:
		return ordHeight(sx, sy, false, iorder)
	case HeightAsc:
		return ordHeight(sx, sy, true, iorder)
	case AreaDesc:
		return ordArea(sx, sy, false, iorder)
	case AreaAsc:
		return ordArea(sx, sy, true, iorder)
	case PerimeterDesc:
		return ordPerimeter(sx, sy, false, iorder)
	case PerimeterAsc:
		return ordPerimeter(sx, sy, true, iorder)
	case DifferenceDesc:
		return ordDifference(sx, sy, false, iorder)
	case DifferenceAsc:
		return ordDifference(sx, sy, true, iorder)
	case RatioDesc:
		return ordRatio(sx, sy, false, iorder)
	case RatioAsc:
		return ordRatio(sx, sy, true, iorder)
	}
	return iorder
}

func (r rslice) Swap(i, j int) {
	r.rects[i], r.rects[j] = r.rects[j], r.rects[i]
}

// A SortedPacker sorts rectangles before packing them.
type SortedPacker struct {
	Packer
	Order Order
}

// Name implements the Packer interface.
func (p *SortedPacker) Name() string {
	return p.Packer.Name() + "." + p.Order.String()
}

func (p *SortedPacker) sort(rects []inrect) {
	sort.Sort(rslice{rects: rects, order: p.Order})
}
