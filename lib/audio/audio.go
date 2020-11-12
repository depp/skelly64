package main

import (
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
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
	a, err := aiff.Parse(data)
	if err != nil {
		return err
	}
	fmt.Println(filename)
	for _, ck := range a.Chunks {
		if r, ok := ck.(*aiff.RawChunk); ok {
			fmt.Printf("RAW: %q\n", r.ID[:])
		} else {
			fmt.Printf("Chunk: %T\n", ck)
		}
	}
	outfile := getpath.GetPath(args[1])
	ext := filepath.Ext(outfile)
	switch strings.ToLower(ext) {
	case ".aif", ".aiff":
		data, err = a.Write(false)
		if err != nil {
			return err
		}
	case ".aifc":
		data, err = a.Write(true)
		if err != nil {
			return err
		}
	default:
		return fmt.Errorf("unknown output file extension: %q", ext)
	}

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
