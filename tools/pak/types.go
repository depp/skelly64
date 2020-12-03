package main

import (
	"fmt"
	"strconv"
	"strings"
)

type datatype uint32

const (
	typeUnknown datatype = iota
	typeData
	typeTrack
	typeModel
	typeTexture
)

type typeinfo struct {
	name      string // Name of type, as it appears in manifest
	slotCount int    // Number of pak slots each asset uses.
}

var types = [...]typeinfo{
	typeData:    {"data", 1},
	typeModel:   {"model", 1},
	typeTexture: {"texture", 1},
	typeTrack:   {"track", 2},
}

func parseType(s string) (t datatype, err error) {
	if s != "" {
		for i, ifo := range types {
			if strings.EqualFold(s, ifo.name) {
				return datatype(i), nil
			}
		}
	}
	return typeUnknown, fmt.Errorf("unknown data type: %q", s)
}

func (t datatype) name() (s string) {
	i := uint32(t)
	if i < uint32(len(types)) {
		s = types[i].name
	}
	if s == "" {
		panic("invalid type: " + strconv.FormatUint(uint64(i), 10))
	}
	return
}

func (t datatype) slotCount() (n int) {
	i := uint32(t)
	if i < uint32(len(types)) {
		n = types[i].slotCount
	}
	if n == 0 {
		panic("invalid type: " + strconv.FormatUint(uint64(i), 10))
	}
	return
}
