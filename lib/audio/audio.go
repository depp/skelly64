package main

import (
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"thornmarked/tools/audio/aiff"
	"thornmarked/tools/getpath"
)

func mainE() error {
	flag.Parse()
	args := flag.Args()
	if len(args) != 2 {
		return errors.New("need two args")
	}
	filename := getpath.GetPath(args[0])
	data, err := ioutil.ReadFile(filename)
	if err != nil {
		return err
	}
	cks, err := aiff.Parse(data)
	if err != nil {
		return err
	}
	fmt.Println(filename)
	for _, ck := range cks {
		if r, ok := ck.(*aiff.RawChunk); ok {
			fmt.Printf("RAW: %q\n", r.ID[:])
		} else {
			fmt.Printf("Chunk: %T\n", ck)
		}
	}
	data, err = aiff.Write(cks)
	if err != nil {
		return err
	}
	outfile := getpath.GetPath(args[1])
	if err := ioutil.WriteFile(outfile, data, 0666); err != nil {
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
