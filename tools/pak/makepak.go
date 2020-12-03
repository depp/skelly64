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

type section struct {
	dtype     datatype
	Entries   []*entry
	UpperName string // Upper case name.
	Type      string // C type name.
	Start     int    // Index of first asset of this type.
}

type manifest struct {
	Size     int
	Sections []*section
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
	fp, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer fp.Close()
	sc := bufio.NewScanner(fp)
	sections := make([]*section, len(types))
	for tidx, tinfo := range types {
		if tinfo.name != "" {
			sections[tidx] = &section{
				dtype:     datatype(tidx),
				UpperName: strings.ToUpper(tinfo.name),
				Type:      "pak_" + tinfo.name,
			}
		}
	}
	for lineno := 1; sc.Scan(); lineno++ {
		e, err := parseManifestLine(sc.Bytes())
		if err != nil {
			return nil, fmt.Errorf("%s:%d: %w", filename, lineno, err)
		}
		if e != nil {
			sec := sections[e.dtype]
			e.Index = len(sec.Entries) + 1 // IDs start at 1.
			sec.Entries = append(sec.Entries, e)
		}
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	pos := 1 // First entry is empty.
	var spos int
	for tidx, tinfo := range types {
		if sec := sections[tidx]; sec != nil {
			sec.Start = pos
			pos += len(sec.Entries) * tinfo.slotCount
			sections[spos] = sec
			spos++
		}
	}
	sections = sections[:spos]
	return &manifest{
		Size:     pos,
		Sections: sections,
	}, nil
}

func (e *entry) resolveInputs(dirs []string) error {
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
	return nil
}

func (sec *section) resolveInputs(dirs []string) error {
	for _, e := range sec.Entries {
		if err := e.resolveInputs(dirs); err != nil {
			return err
		}
	}
	return nil
}

func (mn *manifest) resolveInputs(dirs []string) error {
	for _, sec := range mn.Sections {
		if err := sec.resolveInputs(dirs); err != nil {
			return err
		}
	}
	return nil
}

func mainE() error {
	var dirs []string
	flag.Var(&stringArrayValue{value: &dirs}, "dir", "search for files in")
	manifestFlag := flag.String("manifest", "", "input manifest file")
	dataFlag := flag.String("out-data", "", "output data file")
	codeDirFlag := flag.String("out-code-dir", "", "output directory for code")
	codePrefixFlag := flag.String("out-code-prefix", "", "prefix for output code filenames")
	flag.Parse()
	if *manifestFlag == "" {
		fmt.Fprint(os.Stderr,
			"Usage:\n"+
				"  makepak [-dir <dir> ...] -manifest=<manifest>\n"+
				"          [-out-data=<file>] [-out-code-dir=<file>] [-out-code-prefix=<prefix>]\n")
		os.Exit(1)
	}
	if args := flag.Args(); len(args) != 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	inManifest := getpath.GetPath(*manifestFlag)
	outData := getpath.GetPath(*dataFlag)
	outCode := getpath.GetPath(*codeDirFlag)
	if inManifest == "" {
		return errors.New("missing required flag -manifest")
	}

	mn, err := readManifest(inManifest)
	if err != nil {
		return err
	}
	if outCode != "" {
		if err := mn.writeCode(outCode, *codePrefixFlag); err != nil {
			return err
		}
	}
	if outData != "" {
		if err := mn.resolveInputs(dirs); err != nil {
			return err
		}
		if err := mn.writeData(outData); err != nil {
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
