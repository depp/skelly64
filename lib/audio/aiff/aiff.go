package aiff

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
)

// An AIFF is a decoded AIFF or AIFF-C file.
type AIFF struct {
	Common        Common
	FormatVersion FormatVersion
	Data          *SoundData
	Chunks        []Chunk
}

// IsCompressed returns true if this is a compressed AIFF file.
func (a *AIFF) IsCompressed() bool {
	return !bytes.Equal(a.Common.Compression[:], []byte("NONE"))
}

// A Chunk is a piece of data in an AIFF file.
type Chunk interface {
	ParseChunk(data []byte, compressed bool) error
	ChunkData(compressed bool) (id [4]byte, data []byte, err error)
}

// A RawChunk is a raw chunk which has not been decoded or interpreted.
type RawChunk struct {
	ID   [4]byte
	Data []byte
}

// ParseChunk implements the Chunk interface.
func (c *RawChunk) ParseChunk(data []byte, _ bool) error {
	d := make([]byte, len(data))
	copy(d, data)
	c.Data = d
	return nil
}

// ChunkData implements the Chunk interface.
func (c *RawChunk) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	id = c.ID
	data = c.Data
	return
}

// A Common is the common chunk in an AIFF file.
type Common struct {
	NumChannels     int
	NumFrames       int
	SampleSize      int
	SampleRate      [10]byte
	Compression     [4]byte
	CompressionName string
}

// ParseChunk implements the Chunk interface.
func (c *Common) ParseChunk(data []byte, compressed bool) error {
	if compressed {
		if len(data) < 23 {
			return fmt.Errorf("invalid common chunk: len = %d, should be at least 23", len(data))
		}
	} else {
		if len(data) != 18 {
			return fmt.Errorf("invalid common chunk: len = %d, should be 18", len(data))
		}
	}
	c.NumChannels = int(binary.BigEndian.Uint16(data[0:2]))
	c.NumFrames = int(binary.BigEndian.Uint32(data[2:6]))
	c.SampleSize = int(binary.BigEndian.Uint16(data[6:8]))
	copy(c.SampleRate[:], data[8:])
	if compressed {
		copy(c.Compression[:], data[18:22])
		n := int(data[22])
		rest := data[23:]
		if n != len(rest) {
			return errors.New("invalid string in common chunk")
		}
		c.CompressionName = string(rest)
	} else {
		copy(c.Compression[:], "NONE")
		c.CompressionName = "not compressed"
	}
	return nil
}

// ChunkData implements the Chunk interface.
func (c *Common) ChunkData(compressed bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "COMM")
	if compressed {
		if len(c.CompressionName) > 255 {
			return id, data, errors.New("compression name is too long")
		}
		data = make([]byte, 23+len(c.CompressionName))
	} else {
		if !bytes.Equal(c.Compression[:], []byte("NONE")) {
			return id, data, errors.New("data is compressed, must use AIFF-C format")
		}
		data = make([]byte, 18)
	}
	binary.BigEndian.PutUint16(data[0:2], uint16(c.NumChannels))
	binary.BigEndian.PutUint32(data[2:6], uint32(c.NumFrames))
	binary.BigEndian.PutUint16(data[6:8], uint16(c.SampleSize))
	copy(data[8:], c.SampleRate[:])
	if compressed {
		copy(data[18:22], c.Compression[:])
		data[22] = byte(len(c.CompressionName))
		copy(data[23:], c.CompressionName)
	}
	return
}

// A FormatVersion is the FVER chunk in an AIFC file.
type FormatVersion struct {
	Timestamp uint32
}

// ParseChunk implements the Chunk interface.
func (c *FormatVersion) ParseChunk(data []byte, compressed bool) error {
	if !compressed {
		return errors.New("unexpected FVER in uncompressed file")
	}
	if len(data) != 4 {
		return fmt.Errorf("FVER is length %d, should be length 4", len(data))
	}
	c.Timestamp = binary.BigEndian.Uint32(data)
	return nil
}

// ChunkData implements the Chunk interface.
func (c *FormatVersion) ChunkData(compressed bool) (id [4]byte, data []byte, err error) {
	if !compressed {
		return id, data, errors.New("cannot write FVER to compressed file")
	}
	copy(id[:], "FVER")
	data = make([]byte, 4)
	binary.BigEndian.PutUint32(data, c.Timestamp)
	return
}

// A SoundData is the SSND chunk in an AIFF file.
type SoundData struct {
	Data []byte
}

// ParseChunk implements the Chunk interface.
func (c *SoundData) ParseChunk(data []byte, _ bool) error {
	d := make([]byte, len(data))
	copy(d, data)
	c.Data = d
	return nil
}

// ChunkData implements the Chunk interface.
func (c *SoundData) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "SSND")
	data = c.Data
	return
}

var errUnexpectedEOF = errors.New("unexpected end of file in AIFF data")

// Parse an AIFF or AIFF-C file.
func Parse(data []byte) (*AIFF, error) {
	if len(data) < 12 {
		return nil, errors.New("AIFF too short")
	}
	header := data[0:12:12]
	if !bytes.Equal(header[0:4], []byte("FORM")) {
		return nil, errors.New("not an AIFF file")
	}
	var compressed bool
	switch {
	case bytes.Equal(header[8:12], []byte("AIFF")):
	case bytes.Equal(header[8:12], []byte("AIFC")):
		compressed = true
	default:
		return nil, errors.New("not an AIFF file")
	}
	flen := binary.BigEndian.Uint32(header[4:8])
	if int(flen) < len(data)-8 {
		return nil, errors.New("AIFF file shorter than header indicates")
	}
	rest := data[12:]
	var chunks []Chunk
	var a AIFF
	var hasCommon, hasFVer bool
	for len(rest) > 0 {
		if len(rest) < 8 {
			return nil, errUnexpectedEOF
		}
		ch := rest[0:8:8]
		rest = rest[8:]
		clen := binary.BigEndian.Uint32(ch[4:])
		if int(clen) > len(rest) {
			return nil, errUnexpectedEOF
		}
		cdata := rest[:clen]
		rest = rest[clen:]
		if clen&1 != 0 {
			if len(rest) == 0 {
				return nil, errUnexpectedEOF
			}
			rest = rest[1:]
		}
		var ck Chunk
		switch string(ch[:4]) {
		case "COMM":
			if hasCommon {
				return nil, errors.New("multiple common chunks")
			}
			ck = &a.Common
			hasCommon = true
		case "FVER":
			if hasFVer {
				return nil, errors.New("multiple format version chunks")
			}
			ck = &a.FormatVersion
			hasFVer = true
		case "SSND":
			if a.Data != nil {
				return nil, errors.New("multiple sound data chunks")
			}
			d := new(SoundData)
			a.Data = d
			ck = d
		default:
			r := new(RawChunk)
			copy(r.ID[:], ch[:4])
			ck = r
		}
		if err := ck.ParseChunk(cdata, compressed); err != nil {
			return nil, fmt.Errorf("could not parse %q chunk: %w", ch[:4], err)
		}
		chunks = append(chunks, ck)
	}
	if !hasCommon {
		return nil, errors.New("missing common chunk")
	}
	if compressed && !hasFVer {
		return nil, errors.New("missing FVER chunk")
	}
	if a.Data == nil {
		return nil, errors.New("missign data chunk")
	}
	a.Chunks = chunks
	return &a, nil
}

func (a *AIFF) Write(compressed bool) ([]byte, error) {
	if !compressed && a.IsCompressed() {
		return nil, errors.New("compressed files cannot be written as AIFF")
	}
	rchunks := make([]RawChunk, 0, len(a.Chunks)+1)

	if compressed {
		var id [4]byte
		copy(id[:], "FVER")
		var data [4]byte
		binary.BigEndian.PutUint32(data[:], 0xA2805140)
		rchunks = append(rchunks, RawChunk{id, data[:]})
	}
	for _, ck := range a.Chunks {
		if _, ok := ck.(*FormatVersion); ok {
			continue
		}
		id, data, err := ck.ChunkData(compressed)
		if err != nil {
			return nil, err
		}
		rchunks = append(rchunks, RawChunk{id, data})
	}
	pos := 12
	for _, ck := range rchunks {
		pos = (pos + 8 + len(ck.Data) + 1) &^ 1
	}
	data := make([]byte, pos)
	header := data[0:12:12]
	copy(header[:], "FORM")
	binary.BigEndian.PutUint32(header[4:8], uint32(pos-8))
	if compressed {
		copy(header[8:], "AIFC")
	} else {
		copy(header[8:], "AIFF")
	}
	pos = 12
	for _, ck := range rchunks {
		cheader := data[pos : pos+8 : pos+8]
		copy(cheader[:4], ck.ID[:])
		dlen := (len(ck.Data) + 1) &^ 1
		binary.BigEndian.PutUint32(cheader[4:], uint32(dlen))
		pos += 8
		copy(data[pos:], ck.Data)
		pos += dlen
	}
	return data, nil
}

// GetSamples16 returns the samples in an AIFF file, converted to signed-16 bit.
// Not all conversions are supported.
func (a *AIFF) GetSamples16() ([]int16, error) {
	switch string(a.Common.Compression[:]) {
	case "NONE":
		switch a.Common.SampleSize {
		case 16:
			raw := a.Data.Data
			dec := make([]int16, len(raw)/2)
			for i := 0; i < len(dec); i++ {
				dec[i] = int16(binary.BigEndian.Uint16(raw[i*2 : i*2+2]))
			}
			return dec, nil
		default:
			return nil, fmt.Errorf("unsupported bit depth: %d", a.Common.SampleSize)
		}
	default:
		return nil, fmt.Errorf("unsupported compression: %q", a.Common.Compression[:])
	}
}
