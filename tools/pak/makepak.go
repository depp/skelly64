package main

import (
	"bufio"
	"bytes"
	"errors"
	"flag"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"regexp"
	"strings"
	"unicode/utf8"

	"thornmarked/tools/getpath"
)

type stringArrayValue struct {
	value   *[]string
	changed bool
}

func (v *stringArrayValue) String() string {
	if v.value == nil {
		return ""
	}
	return strings.Join(*v.value, ",")
}

func (v *stringArrayValue) Set(s string) error {
	if v.changed {
		*v.value = append(*v.value, s)
	} else {
		*v.value = []string{s}
		v.changed = true
	}
	return nil
}

func (v *stringArrayValue) Get() interface{} {
	return *v.value
}

// =============================================================================

var validIdent = regexp.MustCompile("^[A-Za-z][A-Za-z0-9_]*$")

type entry struct {
	dtype    datatype
	Ident    string
	Index    int
	filename string
	fullpath string
}

type manifest struct {
	Size    int
	Entries []*entry
}

func parseManifestLine(line []byte) (*entry, error) {
	if !utf8.Valid(line) {
		return nil, errors.New("invalid UTF-8")
	}
	if i := bytes.IndexByte(line, '#'); i != -1 {
		line = line[:i]
	}
	fields := bytes.Fields(line)
	if len(fields) == 0 {
		return nil, nil
	}
	if len(fields) != 3 {
		return nil, fmt.Errorf("got %d fields, expected exactly 3", len(fields))
	}
	dt, err := parseType(string(fields[0]))
	if err != nil {
		return nil, err
	}
	ident := string(fields[1])
	if !validIdent.MatchString(ident) {
		return nil, fmt.Errorf("invalid identifier: %q", ident)
	}
	filename := path.Clean(string(fields[2]))
	if path.IsAbs(filename) {
		return nil, errors.New("path is absolute")
	}
	return &entry{
		dtype:    dt,
		Ident:    ident,
		filename: filename,
	}, nil
}

func readManifest(filename string) (*manifest, error) {
	var entries []*entry
	fp, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer fp.Close()
	sc := bufio.NewScanner(fp)
	size := 1
	for lineno := 1; sc.Scan(); lineno++ {
		e, err := parseManifestLine(sc.Bytes())
		if err != nil {
			return nil, fmt.Errorf("%s:%d: %w", filename, lineno, err)
		}
		if e != nil {
			sc := e.dtype.slotCount()
			e.Index = size
			entries = append(entries, e)
			size += sc
		}
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	return &manifest{
		Size:    size,
		Entries: entries,
	}, nil
}

func resolveInputs(mn *manifest, dirs []string) error {
	for _, e := range mn.Entries {
		var fullpath string
		for _, dir := range dirs {
			p := filepath.Join(dir, e.filename)
			if _, err := os.Stat(p); err == nil {
				fullpath = p
				break
			}
		}
		if fullpath == "" {
			return fmt.Errorf("could not find file: %q", e.filename)
		}
		e.fullpath = fullpath
	}
	return nil
}

func mainE() error {
	var dirs []string
	flag.Var(&stringArrayValue{value: &dirs}, "dir", "search for files in")
	manifestFlag := flag.String("manifest", "", "input manifest file")
	dataFlag := flag.String("out-data", "", "input manifest file")
	headerFlag := flag.String("out-header", "", "output header file")
	flag.Parse()
	if *manifestFlag == "" {
		fmt.Fprint(os.Stderr,
			"Usage:\n"+
				"  makepak [-dir <dir> ...] -manifest=<manifest>\n"+
				"          -data-out=<file> -header-out=<file>\n")
		os.Exit(1)
	}
	if args := flag.Args(); len(args) != 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	inManifest := getpath.GetPath(*manifestFlag)
	outData := getpath.GetPath(*dataFlag)
	outHeader := getpath.GetPath(*headerFlag)
	if inManifest == "" {
		return errors.New("missing required flag -manifest")
	}

	mn, err := readManifest(inManifest)
	if err != nil {
		return err
	}
	if outHeader != "" {
		if err := writeHeader(outHeader, mn); err != nil {
			return err
		}
	}
	if outData != "" {
		if err := resolveInputs(mn, dirs); err != nil {
			return err
		}
		if err := writeData(outData, mn); err != nil {
			return err
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
