package main

import (
	"flag"
	"fmt"
	"math/rand"
	"os"

	"thornmarked/tools/rectpack"
)

const sizeLimit = 1024

type packer struct {
	packer      rectpack.Packer
	overheadSum float64
	count       int
}

func mainE() error {
	maxsizeArg := flag.Int("maxsize", 32, "maximum size of generated rectangles")
	minsizeArg := flag.Int("minsize", 1, "minimum size of generated rectangles")
	countArg := flag.Int("count", 100, "number of generated rectangles")
	iterArg := flag.Int("iterations", 100, "number of iterations")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	iterCount := *iterArg
	if iterCount < 0 {
		iterCount = 0
	}
	minsize := int32(*minsizeArg)
	if minsize < 1 || sizeLimit < minsize {
		return fmt.Errorf("minsize %d is not between 1 and %d", minsize, sizeLimit)
	}
	maxsize := int32(*maxsizeArg)
	if maxsize < minsize || sizeLimit < maxsize {
		return fmt.Errorf("maxsize %d is not between %d and %d", maxsize, minsize, sizeLimit)
	}
	rects := make([]rectpack.Point, *countArg)
	if minsize == maxsize {
		iterCount = 0
		for i := range rects {
			rects[i] = rectpack.Point{
				X: int32(minsize),
				Y: int32(minsize),
			}
		}
	}
	rnd := rand.New(rand.NewSource(0x1234))
	packers := []*packer{}
	for _, p := range rectpack.AllAlgorithms() {
		packers = append(packers, &packer{packer: p})
	}
	for iter := 0; iter < iterCount; iter++ {
		var area int32
		if minsize != maxsize {
			n := maxsize - minsize + 1
			for i := range rects {
				r := rectpack.Point{
					X: minsize + rnd.Int31n(n),
					Y: minsize + rnd.Int31n(n),
				}
				rects[i] = r
				area += r.X * r.Y
			}
		}
		for _, p := range packers {
			bounds, _, err := rectpack.AutoPackSingle(p.packer, rects)
			if err != nil {
				fmt.Fprintf(os.Stderr, "packer %s failed: %v\n", p.packer.Name(), err)
				continue
			}
			barea := bounds.X * bounds.Y
			overhead := float64(barea-area) / float64(area)
			p.overheadSum += overhead
			p.count++
		}
	}
	if _, err := fmt.Println("Algorithm,Overhead"); err != nil {
		return err
	}
	for _, p := range packers {
		if p.count > 0 {
			if _, err := fmt.Printf("%s,%.5f\n", p.packer.Name(), p.overheadSum/float64(p.count)); err != nil {
				return err
			}
		}
	}
	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintf(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
