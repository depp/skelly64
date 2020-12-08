package audio

import (
	"encoding/binary"
	"errors"
	"fmt"

	"thornmarked/tools/audio/aiff"
)

// A Track is an audio music track.
type Track struct {
	WaveTable []byte // Audio wave table data (ALWaveTable).
	Data      []byte // Sample data.
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

func pad4(d []byte) []byte {
	var b [4]byte
	n := (-len(d)) & 3
	return append(d, b[:n]...)
}

type waveType byte

const (
	adpcmType waveType = 0
	rawType
)

func waveTable(t waveType) []byte {
	d := make([]byte, 20)
	d[8] = byte(t)
	return d
}

func rawWaveTable(a *aiff.AIFF) ([]byte, error) {
	if a.Common.SampleSize != 16 {
		return nil, fmt.Errorf("sample size is %d, but only 16 is supported", a.Common.SampleSize)
	}
	if a.Common.NumChannels != 1 {
		return nil, fmt.Errorf("track has %d channels, but only one is supported", a.Common.NumChannels)
	}
	out := waveTable(rawType)
	return out, nil
}

func adpcmWaveTable(a *aiff.AIFF) ([]byte, error) {
	var loop, codebook []byte
	for _, ck := range a.Chunks {
		switch ck := ck.(type) {
		case *aiff.VADPCMLoops:
			if len(ck.Loops) > 0 {
				if loop != nil {
					return nil, errors.New("multiple loops")
				}
				loop = make([]byte, aiff.VADPCMLoopSize)
				ck.Loops[0].Marshal(loop)
			}
		case *aiff.VADPCMCodes:
			if len(codebook) != 0 {
				return nil, errors.New("multiple codebooks")
			}
			codebook = packCodebook(ck)
		}
	}
	if len(codebook) == 0 {
		return nil, errors.New("ADPCM track has no codebook")
	}
	out := waveTable(adpcmType)
	if loop != nil {
		out = pad4(out)
		binary.BigEndian.PutUint32(out[12:], uint32(len(out)))
		out = append(out, loop...)
	}
	if codebook != nil {
		out = pad4(out)
		binary.BigEndian.PutUint32(out[16:], uint32(len(out)))
		out = append(out, codebook...)
	}
	return out, nil
}

// ReadTrack reads a music track from disk.
func ReadTrack(data []byte) (*Track, error) {
	a, err := aiff.Parse(data)
	if err != nil {
		return nil, err
	}
	var tr Track
	switch string(a.Common.Compression[:]) {
	case "NONE":
		tr.WaveTable, err = rawWaveTable(a)
		if err != nil {
			return nil, err
		}
	case "VAPC":
		tr.WaveTable, err = adpcmWaveTable(a)
		if err != nil {
			return nil, err
		}
	default:
		return nil, errors.New("unsupported audio sample format")
	}
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
		}
	}
	return &tr, nil
}
