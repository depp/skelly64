package aiff

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
)

// A Chunk is a piece of data in an AIFF file.
type Chunk interface {
	ParseChunk(data []byte) error
	ChunkData() (id [4]byte, data []byte, err error)
}

// A RawChunk is a raw chunk which has not been decoded or interpreted.
type RawChunk struct {
	ID   [4]byte
	Data []byte
}

// ParseChunk implements the Chunk interface.
func (c *RawChunk) ParseChunk(data []byte) error {
	d := make([]byte, len(data))
	copy(d, data)
	c.Data = d
	return nil
}

// ChunkData implements the Chunk interface.
func (c *RawChunk) ChunkData() (id [4]byte, data []byte, err error) {
	id = c.ID
	data = c.Data
	return
}

// A Common is the common chunk in an AIFF file.
type Common struct {
	NumChannels int
	NumFrames   int
	SampleSize  int
	SampleRate  [10]byte
}

// ParseChunk implements the Chunk interface.
func (c *Common) ParseChunk(data []byte) error {
	if len(data) != 18 {
		return fmt.Errorf("invalid common chunk: len = %d, should be 18", len(data))
	}
	c.NumChannels = int(binary.BigEndian.Uint16(data[0:2]))
	c.NumFrames = int(binary.BigEndian.Uint32(data[2:6]))
	c.SampleSize = int(binary.BigEndian.Uint16(data[6:8]))
	copy(c.SampleRate[:], data[8:])
	return nil
}

func (c *Common) put(data []byte) {
	data = data[0:18:18]
	binary.BigEndian.PutUint16(data[0:2], uint16(c.NumChannels))
	binary.BigEndian.PutUint32(data[2:6], uint32(c.NumFrames))
	binary.BigEndian.PutUint16(data[6:8], uint16(c.SampleSize))
	copy(data[8:], c.SampleRate[:])
}

// ChunkData implements the Chunk interface.
func (c *Common) ChunkData() (id [4]byte, data []byte, err error) {
	copy(id[:], "COMM")
	data = make([]byte, 18)
	c.put(data)
	return
}

// A CommonC is the common chunk in an AIFF-C file.
type CommonC struct {
	Common
	Compression     [4]byte
	CompressionName string
}

// ParseChunk implements the Chunk interface.
func (c *CommonC) ParseChunk(data []byte) error {
	if len(data) < 23 {
		return fmt.Errorf("invalid common chunk: len = %d, should be at least 23", len(data))
	}
	if err := c.Common.ParseChunk(data[0:18]); err != nil {
		return err
	}
	copy(c.Compression[:], data[18:22])
	n := int(data[22])
	rest := data[23:]
	if n != len(rest) {
		return errors.New("invalid string in common chunk")
	}
	c.CompressionName = string(rest)
	return nil
}

// ChunkData implements the Chunk interface.
func (c *CommonC) ChunkData() (id [4]byte, data []byte, err error) {
	if len(c.CompressionName) > 255 {
		return id, data, errors.New("compression name is too long")
	}
	copy(id[:], "COMM")
	data = make([]byte, 23+len(c.CompressionName))
	c.put(data)
	copy(data[18:22], c.Compression[:])
	data[22] = byte(len(c.CompressionName))
	copy(data[23:], c.CompressionName)
	return
}

// A SoundData is the SSND chunk in an AIFF file.
type SoundData struct {
	Data []byte
}

// ParseChunk implements the Chunk interface.
func (c *SoundData) ParseChunk(data []byte) error {
	d := make([]byte, len(data))
	copy(d, data)
	c.Data = d
	return nil
}

// ChunkData implements the Chunk interface.
func (c *SoundData) ChunkData() (id [4]byte, data []byte, err error) {
	copy(id[:], "SSND")
	data = c.Data
	return
}

var errUnexpectedEOF = errors.New("unexpected end of file in AIFF data")

// Parse an AIFF or AIFF-C file.
func Parse(data []byte) ([]Chunk, error) {
	if len(data) < 12 {
		return nil, errors.New("AIFF too short")
	}
	header := data[0:12:12]
	if !bytes.Equal(header[0:4], []byte("FORM")) {
		return nil, errors.New("not an AIFF file")
	}
	var aiffc bool
	switch {
	case bytes.Equal(header[8:12], []byte("AIFF")):
	case bytes.Equal(header[8:12], []byte("AIFC")):
		aiffc = true
	default:
		return nil, errors.New("not an AIFF file")
	}
	flen := binary.BigEndian.Uint32(header[4:8])
	if int(flen) < len(data)-8 {
		return nil, errors.New("AIFF file shorter than header indicates")
	}
	rest := data[12:]
	var chunks []Chunk
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
			if aiffc {
				ck = new(CommonC)
			} else {
				ck = new(Common)
			}
		default:
			r := new(RawChunk)
			copy(r.ID[:], ch[:4])
			ck = r
		}
		if err := ck.ParseChunk(cdata); err != nil {
			return nil, fmt.Errorf("could not parse %q chunk: %w", ch[:4], err)
		}
		chunks = append(chunks, ck)
	}
	return chunks, nil
}

func isAIFFC(chunks []Chunk) (bool, error) {
	for _, ck := range chunks {
		if _, ok := ck.(*Common); ok {
			return false, nil
		}
		if _, ok := ck.(*CommonC); ok {
			return true, nil
		}
	}
	return false, errors.New("missing common chunk")
}

// Write writes a sequence of chunks as an AIFF or AIFF-C file.
func Write(chunks []Chunk) ([]byte, error) {
	aiffc, err := isAIFFC(chunks)
	if err != nil {
		return nil, err
	}
	rchunks := make([]RawChunk, len(chunks))
	pos := 12
	for i, ck := range chunks {
		id, data, err := ck.ChunkData()
		if err != nil {
			return nil, err
		}
		rchunks[i] = RawChunk{id, data}
		pos = (pos + 8 + len(data) + 1) &^ 1
	}
	data := make([]byte, pos)
	header := data[0:12:12]
	copy(header[:], "FORM")
	binary.BigEndian.PutUint32(header[4:8], uint32(pos-8))
	if aiffc {
		copy(header[8:], "AIFC")
	} else {
		copy(header[8:], "AIFF")
	}
	pos = 12
	for _, ck := range rchunks {
		cheader := data[pos : pos+8 : pos+8]
		copy(cheader[:4], ck.ID[:])
		binary.BigEndian.PutUint32(cheader[4:], uint32(len(ck.Data)))
		pos += 8
		copy(data[pos:], ck.Data)
		pos += len(ck.Data)
		if pos&1 == 1 {
			pos++
		}
	}
	return data, nil
}
