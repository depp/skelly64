package rectpack

import (
	"math/rand"
	"testing"
)

func makeSizes(r *rand.Rand, size, count int) []Point {
	sizes := make([]Point, count)
	for i := range sizes {
		sizes[i] = Point{
			X: 1 + r.Int31n(int32(size)),
			Y: 1 + r.Int31n(int32(size)),
		}
	}
	return sizes
}

func TestPack(t *testing.T) {
	r := rand.New(rand.NewSource(0x1234))
	sizelists := [][]Point{
		makeSizes(r, 50, 50),
		makeSizes(r, 8, 50),
		makeSizes(r, 32, 10),
	}
	packers := []Packer{
		new(MaxRectsBL),
	}
	for _, p := range packers {
		t.Run(p.Name(), func(t *testing.T) {
			var rects []rect
			for _, sizes := range sizelists {
				rects = rects[:0]
				region := rect{
					max: Point{X: 256, Y: 256},
				}
				p.Reset(region.max)
				for _, sz := range sizes {
					if pos, ok := p.AddRect(sz); ok {
						r := rect{
							min: pos,
							max: Point{
								X: pos.X + sz.X,
								Y: pos.Y + sz.Y,
							},
						}
						if !region.contains(r) {
							t.Errorf("rect %v not in region", r)
						}
						for _, e := range rects {
							if e.intersect(r) {
								t.Errorf("rect %v intersects with %v", e, r)
								continue
							}
						}
						rects = append(rects, r)
					}
				}
			}
		})
	}
}
