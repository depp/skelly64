package main

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"thornmarked/tools/font/charset"
)

func hash(x uint32) uint32 {
	const (
		c1 = 0xcc9e2d51
		// c2 = 0x1b873593
	)
	x *= c1
	// x = (x << 15) | (x >> 17)
	// x *= c2
	return x
}

func evalSetSize(cps []int, n int) {
	tab := make([]bool, n)
	dist := make([]int, n)
	var mdist int
	for _, c := range cps {
		h := int(hash(uint32(c)))
		for i := 0; i < n; i++ {
			j := (h + i) & (n - 1)
			if !tab[j] {
				tab[j] = true
				dist[i]++
				if i > mdist {
					mdist = i
				}
				break
			}
		}
	}
	fmt.Println()
	fmt.Println("Size:", n)
	fmt.Println("Max distance:", mdist)
}

func evalSet(cs charset.Set) {
	n := 1
	for n < len(cs)+1 {
		n <<= 1
	}
	cps := make([]int, 0, len(cs))
	for c := range cs {
		cps = append(cps, int(c))
	}
	sort.Ints(cps)
	evalSetSize(cps, n)
	evalSetSize(cps, n*2)
}

func mainE() error {
	wd := os.Getenv("BUILD_WORKING_DIRECTORY")
	if wd != "" {
		if err := os.Chdir(wd); err != nil {
			return err
		}
	}
	flag.Parse()
	args := flag.Args()
	if len(args) == 0 {
		fmt.Fprintln(os.Stderr, "Usage: charseteval <charset>")
		os.Exit(1)
	}
	for _, arg := range args {
		cs, err := charset.ReadFile(arg)
		if err != nil {
			return err
		}
		evalSet(cs)
	}
	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
