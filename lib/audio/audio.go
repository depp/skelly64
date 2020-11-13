package audio

import (
	"encoding/binary"
	"errors"
	"fmt"

	"thornmarked/tools/audio/aiff"
)

// A Track is an audio music track.
type Track struct {
	Codebook []byte // The codebook, or nil for 16-bit sounds.
	Data     []byte // Sample data.
}

func packCodebook(ck *aiff.VADPCMCodes) []byte {
	data := make([]byte, 8+2*len(ck.Table))
	binary.BigEndian.PutUint32(data[:4], uint32(ck.Order))
	binary.BigEndian.PutUint32(data[4:8], uint32(ck.NumEntries))
	for i, x := range ck.Table {
		binary.BigEndian.PutUint16(data[8+2*i:], uint16(x))
	}
	return data
}

// ReadTrack reads a music track from disk.
func ReadTrack(data []byte) (*Track, error) {
	a, err := aiff.Parse(data)
	if err != nil {
		return nil, err
	}
	var adpcm bool
	switch string(a.Common.Compression[:]) {
	case "NONE":
		if a.Common.SampleSize != 16 {
			return nil, fmt.Errorf("sample size is %d, but only 16 is supported", a.Common.SampleSize)
		}
		if a.Common.NumChannels != 1 {
			return nil, fmt.Errorf("track has %d channels, but only one is supported", a.Common.NumChannels)
		}
	case "VAPC":
		adpcm = true
	}
	var tr Track
	for _, ck := range a.Chunks {
		switch ck := ck.(type) {
		case *aiff.SoundData:
			if len(ck.Data) == 0 {
				return nil, errors.New("empty track")
			}
			if len(tr.Data) != 0 {
				return nil, errors.New("multiple sound data chunks")
			}
			tr.Data = ck.Data
		case *aiff.VADPCMCodes:
			if len(tr.Codebook) != 0 {
				return nil, errors.New("multiple codebooks")
			}
			if !adpcm {
				return nil, errors.New("unexpected codebook")
			}
			tr.Codebook = packCodebook(ck)
		}
	}
	if adpcm && len(tr.Codebook) == 0 {
		return nil, errors.New("ADPCM track has no codebook")
	}
	return &tr, nil
}
