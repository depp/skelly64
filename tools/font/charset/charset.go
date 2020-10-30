package charset

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"os"
	"strconv"
	"unicode/utf8"
)

// A Set is a set of Unicode characters.
type Set map[uint32]bool

func parseChar(b []byte) (uint32, error) {
	if b[0] == '$' {
		rest := string(b[1:])
		if rest == "$" {
			return '$', nil
		}
		v, err := strconv.ParseInt(rest, 16, 32)
		if err != nil {
			return 0, err
		}
		if v > utf8.MaxRune {
			return 0, fmt.Errorf("code point is too large: 0x%x", v)
		}
		return uint32(v), nil
	}
	v, n := utf8.DecodeRune(b)
	if v == utf8.RuneError && n <= 1 {
		return 0, fmt.Errorf("invalid UTF-8: %q", b)
	}
	if n < len(b) {
		return 0, fmt.Errorf("multiple characters: %q", b)
	}
	return uint32(v), nil
}

func readLine(s Set, line []byte) error {
	if i := bytes.IndexByte(line, '#'); i != -1 {
		line = line[:i]
	}
	fs := bytes.Fields(line)
	if len(fs) == 0 {
		return nil
	}
	cmd := string(fs[0])
	args := fs[1:]
	switch cmd {
	case "range":
		if len(args) != 2 {
			return fmt.Errorf("range has %d arguments, expected exactly 2", len(args))
		}
		c1, err := parseChar(args[0])
		if err != nil {
			return err
		}
		c2, err := parseChar(args[1])
		if err != nil {
			return err
		}
		if c1 > c2 {
			return fmt.Errorf("range is backwards: %#x > %#x", c1, c2)
		}
		for c := c1; c <= c2; c++ {
			s[c] = true
		}
		return nil
	case "chars":
		if len(args) == 0 {
			return errors.New("missing arguments for chars")
		}
		for _, arg := range args {
			c, err := parseChar(arg)
			if err != nil {
				return err
			}
			s[c] = true
		}
		return nil
	default:
		return fmt.Errorf("unknown command: %q", cmd)
	}
}

// ReadFile reads a character set from a file.
func ReadFile(filename string) (Set, error) {
	fp, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer fp.Close()
	s := make(Set)
	sc := bufio.NewScanner(fp)
	for lineno := 1; sc.Scan(); lineno++ {
		if err := readLine(s, sc.Bytes()); err != nil {
			return nil, fmt.Errorf("%s:%d: %w", filename, lineno, err)
		}
	}
	if err := sc.Err(); err != nil {
		return nil, err
	}
	return s, nil
}
