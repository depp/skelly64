package metadata

import (
	"encoding/json"
	"fmt"
	"os"
)

// A Metadata contains metadata associated with an audio file.
type Metadata struct {
	LeadIn     float64  `json:"leadIn"`
	LoopLength *float64 `json:"loopLength"`
}

// Read reads the metadata JSON file.
func Read(filename string) (md Metadata, err error) {
	fp, err := os.Open(filename)
	if err != nil {
		return md, fmt.Errorf("could not read metadata: %v", err)
	}
	defer fp.Close()
	dec := json.NewDecoder(fp)
	dec.DisallowUnknownFields()
	if err := dec.Decode(&md); err != nil {
		return md, fmt.Errorf("could not parse metadata file %q: %v", filename, err)
	}
	return md, nil
}
