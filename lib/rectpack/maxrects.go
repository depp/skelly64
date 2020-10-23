package rectpack

import "math"

// A MaxRects is a structure that tracks free space in a rectangle.
type MaxRects struct {
	free []rect
}

// Reset resets the structure to contain free space with the given bounds.
func (p *MaxRects) Reset(bounds Point) {
	p.free = append(p.free[:0], rect{max: bounds})
}

// PlaceRect places a rectangle in the packing, removing the rectangle from free
// space.
func (p *MaxRects) PlaceRect(r rect) {
	free := p.free
	// Rects from 0..pos have not been affected by the split.
	// Rects from pos..unsplit have not been split.
	// Rects from unsplit..len(free) are the result of splitting.
	var pos int
	unsplit := len(free)
	for pos < unsplit {
		f := free[pos]
		if !r.intersect(f) {
			// Rect is unaffected.
			pos++
			continue
		}
		// Remove this rectangle and add the splits.
		unsplit--
		free[pos] = free[unsplit]
		free[unsplit] = free[len(free)-1]
		free = free[:len(free)-1]
		var splits [4]rect
		for i := range splits {
			splits[i] = f
		}
		var nsplit int
		if r.max.X < f.max.X {
			splits[nsplit].min.X = r.max.X
			nsplit++
		}
		if r.max.Y < f.max.Y {
			splits[nsplit].min.Y = r.max.Y
			nsplit++
		}
		if r.min.X > f.min.X {
			splits[nsplit].max.X = r.min.X
			nsplit++
		}
		if r.min.Y > f.min.Y {
			splits[nsplit].max.Y = r.min.Y
			nsplit++
		}
		start := len(free)
	next_split:
		for _, split := range splits[:nsplit] {
			j := start
			for j > unsplit {
				j--
				check := free[j]
				if check.contains(split) {
					continue next_split
				}
				if split.contains(check) {
					free[j] = split
					continue next_split
				}
			}
			for j > 0 {
				j--
				check := free[j]
				if check.contains(split) {
					continue next_split
				}
			}
			free = append(free, split)
		}
	}
	p.free = free
}

// MaxRectsBL (MaxRects, bottom-left) packer packs rectangles into a rectangle,
// minimizing the Y coordinate of each rect.
type MaxRectsBL struct {
	free MaxRects
}

// Name implements the Packer interface.
func (*MaxRectsBL) Name() string {
	return "MaxRects.BL"
}

// Reset implements the Packer interface.
func (p *MaxRectsBL) Reset(bounds Point) {
	p.free.Reset(bounds)
}

// AddRect implements the Packer interface.
func (p *MaxRectsBL) AddRect(size Point) (pos Point, ok bool) {
	pos = Point{X: 0, Y: math.MaxInt32}
	for _, f := range p.free.free {
		// If it fits AND it is either a better Y coordinate, or it is tied for
		// lowest Y coordinate and has a lower X coordinate.
		if size.X <= f.max.X-f.min.X &&
			size.Y <= f.max.Y-f.min.Y &&
			(f.min.Y < pos.Y || f.min.Y == pos.Y && f.min.X < pos.X) {
			pos = f.min
			ok = true
		}
	}
	if ok {
		p.free.PlaceRect(rect{
			min: pos,
			max: Point{
				X: pos.X + size.X,
				Y: pos.Y + size.Y,
			},
		})
	}
	return
}

func (p *MaxRectsBL) addRects(rs *rectSet) {
	defaultAddRects(p, rs)
}

func (*MaxRectsBL) sort(_ []inrect) {}

func (*MaxRectsBL) isYOrdered() bool {
	return true
}
