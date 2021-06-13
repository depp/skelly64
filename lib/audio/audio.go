package audio

import (
	"encoding/binary"
	"errors"
	"fmt"

	"github.com/depp/skelly64/lib/audio/aiff"
)

// A Track is an audio music track.
type Track struct {
	Header []byte // Audio header data (struct audio_header).
	Data   []byte // Sample data.
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

func baseHeader(t waveType) []byte {
	d := make([]byte, 28)
	d[8] = byte(t)
	return d
}

func rawHeader(a *aiff.AIFF) ([]byte, error) {
	if a.Common.SampleSize != 16 {
		return nil, fmt.Errorf("sample size is %d, but only 16 is supported", a.Common.SampleSize)
	}
	if a.Common.NumChannels != 1 {
		return nil, fmt.Errorf("track has %d channels, but only one is supported", a.Common.NumChannels)
	}
	out := baseHeader(rawType)
	return out, nil
}

func adpcmHeader(a *aiff.AIFF) ([]byte, error) {
	out := baseHeader(adpcmType)
	var loop, codebook []byte
	for _, ck := range a.Chunks {
		switch ck := ck.(type) {
		case *aiff.VADPCMLoops:
			if len(ck.Loops) > 0 {
				if loop != nil {
					return nil, errors.New("multiple loops")
				}
				loop = make([]byte, aiff.VADPCMLoopSize)
				l := ck.Loops[0]
				l.Marshal(loop)
				binary.BigEndian.PutUint32(out[4:], uint32(l.End-l.Start))
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
	if loop != nil {
		out = pad4(out)
		binary.BigEndian.PutUint32(out[20:], uint32(len(out)))
		out = append(out, loop...)
	}
	if codebook != nil {
		out = pad4(out)
		binary.BigEndian.PutUint32(out[24:], uint32(len(out)))
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
		tr.Header, err = rawHeader(a)
		if err != nil {
			return nil, err
		}
	case "VAPC":
		tr.Header, err = adpcmHeader(a)
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
		case *aiff.Markers:
			for _, m := range ck.Markers {
				switch m.Name {
				case "LeadIn":
					binary.BigEndian.PutUint32(tr.Header, uint32(m.Position))
				}
			}
		}
	}
	return &tr, nil
}
