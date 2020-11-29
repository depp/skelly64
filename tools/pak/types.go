package main

import (
	"fmt"
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

var types = [...]string{
	typeData:    "data",
	typeTrack:   "track",
	typeModel:   "model",
	typeTexture: "texture",
}

var typeSlotCount = [...]int{
	typeData:    1,
	typeTrack:   2,
	typeModel:   1,
	typeTexture: 1,
}

func parseType(s string) (t datatype, err error) {
	if s != "" {
		for i, ts := range types {
			if strings.EqualFold(s, ts) {
				return datatype(i), nil
			}
		}
	}
	return typeUnknown, fmt.Errorf("unknown data type: %q", s)
}
