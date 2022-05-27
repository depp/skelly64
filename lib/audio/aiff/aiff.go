package aiff

import (
	"encoding/binary"
	"errors"
	"fmt"

	"github.com/depp/extended"
)

// StardardVersion is the recognized AIFF-C version number.
const StandardVersion = 0xA2805140

// An AIFF is a decoded AIFF or AIFF-C file.
type AIFF struct {
	Common        Common
	FormatVersion FormatVersion
	Data          *SoundData
	Chunks        []Chunk
}

// IsCompressed returns true if this is a compressed AIFF file.
func (a *AIFF) IsCompressed() bool {
	return a.Common.IsComprsesed()
}

// A Chunk is a piece of data in an AIFF file.
type Chunk interface {
	ChunkData(compressed bool) (id [4]byte, data []byte, err error)
}

type parsableChunk interface {
	Chunk
	parseChunk(data []byte, compressed bool) error
}

// =============================================================================

// A RawChunk is a raw chunk which has not been decoded or interpreted.
type RawChunk struct {
	ID   [4]byte
	Data []byte
}

// parseChunk implements the Chunk interface.
func (c *RawChunk) parseChunk(data []byte, _ bool) error {
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

// =============================================================================

const (
	// PCMType is the compression type for PCM (uncompressed) audio.
	PCMType = "NONE"

	// PCMName is the descriptive name for PCM (uncompressed) audio.
	PCMName = "not compressed"
)

// A Common is the common chunk in an AIFF file.
type Common struct {
	NumChannels     int
	NumFrames       int
	SampleSize      int
	SampleRate      extended.Extended
	Compression     [4]byte
	CompressionName string
}

// parseChunk implements the Chunk interface.
func (c *Common) parseChunk(data []byte, compressed bool) error {
	if compressed {
		if len(data) < 23 {
			return fmt.Errorf("invalid common chunk: len = %d, should be at least 23", len(data))
		}
	} else {
		if len(data) != 18 {
			return fmt.Errorf("invalid common chunk: len = %d, should be 18", len(data))
		}
	}
	_ = data[:18]
	c.NumChannels = int(binary.BigEndian.Uint16(data[0:2]))
	c.NumFrames = int(binary.BigEndian.Uint32(data[2:6]))
	c.SampleSize = int(binary.BigEndian.Uint16(data[6:8]))
	c.SampleRate = extended.FromBytesBigEndian(data[8:])
	if compressed {
		copy(c.Compression[:], data[18:22])
		n := int(data[22])
		rest := data[23:]
		if n != len(rest) {
			return errors.New("invalid string in common chunk")
		}
		c.CompressionName = string(rest)
	} else {
		copy(c.Compression[:], PCMType)
		c.CompressionName = PCMName
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
		if c.IsComprsesed() {
			return id, data, errors.New("data is compressed, must use AIFF-C format")
		}
		data = make([]byte, 18)
	}
	binary.BigEndian.PutUint16(data[0:2], uint16(c.NumChannels))
	binary.BigEndian.PutUint32(data[2:6], uint32(c.NumFrames))
	binary.BigEndian.PutUint16(data[6:8], uint16(c.SampleSize))
	c.SampleRate.PutBytesBigEndian(data[8:])
	if compressed {
		copy(data[18:22], c.Compression[:])
		data[22] = byte(len(c.CompressionName))
		copy(data[23:], c.CompressionName)
	}
	return
}

// IsCompressed returns true if this belongs to a compressed AIFF file.
func (c *Common) IsComprsesed() bool {
	return string(c.Compression[:]) != PCMType
}

// =============================================================================

// A FormatVersion is the FVER chunk in an AIFC file.
type FormatVersion struct {
	Timestamp uint32
}

// parseChunk implements the Chunk interface.
func (c *FormatVersion) parseChunk(data []byte, compressed bool) error {
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

// =============================================================================

// A SoundData is the SSND chunk in an AIFF file.
type SoundData struct {
	Offset    uint32
	BlockSize uint32
	Data      []byte
}

// parseChunk implements the Chunk interface.
func (c *SoundData) parseChunk(data []byte, _ bool) error {
	if len(data) < 8 {
		return errors.New("sound data chunk too short")
	}
	c.Offset = binary.BigEndian.Uint32(data[:4])
	c.BlockSize = binary.BigEndian.Uint32(data[4:8])
	d := make([]byte, len(data)-8)
	copy(d, data[8:])
	c.Data = d
	return nil
}

// ChunkData implements the Chunk interface.
func (c *SoundData) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "SSND")
	data = make([]byte, len(c.Data)+8)
	binary.BigEndian.PutUint32(data[0:4], c.Offset)
	binary.BigEndian.PutUint32(data[4:8], c.BlockSize)
	copy(data[8:], c.Data)
	return
}

// =============================================================================

// A Marker is a named location in an AIFF file.
type Marker struct {
	ID       int
	Position int
	Name     string
}

// A Markers is the MARK chunk in an AIFF file.
type Markers struct {
	Markers []Marker
}

func (c *Markers) parseChunk(data []byte, _ bool) error {
	count := int(binary.BigEndian.Uint16(data))
	c.Markers = make([]Marker, count)
	d := data[2:]
	for i := range c.Markers {
		if len(d) < 6 {
			return errUnexpectedEOF
		}
		id := int(binary.BigEndian.Uint16(d))
		pos := int(binary.BigEndian.Uint32(d[2:]))
		n := int(d[6])
		sz := (7 + n + +1) &^ 1
		if len(d) < sz {
			return errUnexpectedEOF
		}
		c.Markers[i] = Marker{
			ID:       id,
			Position: pos,
			Name:     string(d[7 : 7+n]),
		}
		d = d[sz:]
	}
	return nil
}

// ChunkData implements the Chunk interface.
func (c *Markers) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "MARK")
	n := 2
	for _, m := range c.Markers {
		if len(m.Name) > 255 {
			return id, data, fmt.Errorf("marker name too long, must be 255 bytes or less: %q", m.Name)
		}
		n += (7 + len(m.Name) + 1) &^ 1
	}
	data = make([]byte, n)
	binary.BigEndian.PutUint16(data, uint16(len(c.Markers)))
	d := data[2:]
	for _, m := range c.Markers {
		binary.BigEndian.PutUint16(d, uint16(m.ID))
		binary.BigEndian.PutUint32(d[2:], uint32(m.Position))
		d[6] = byte(len(m.Name))
		copy(d[7:], m.Name)
		d = d[(7+len(m.Name)+1)&^1:]
	}
	return id, data, nil
}

// =============================================================================

// A LoopMode describes how a loop is played back.
type LoopMode int

const (
	// LoopNone does not loop.
	LoopNone LoopMode = 0
	// LoopForward plays the loop forwards repeatedly.
	LoopForward LoopMode = 1
	// LoopForwardBackward alternates between playing the loop forwards and
	// backwards.
	LoopForwardBackward LoopMode = 2
)

// A Loop is an audio loop in an AIFF file.
type Loop struct {
	Mode  LoopMode
	Begin int // Marker ID.
	End   int // Marker ID.
}

func (l *Loop) parse(data []byte) {
	l.Mode = LoopMode(binary.BigEndian.Uint16(data))
	l.Begin = int(binary.BigEndian.Uint16(data[2:]))
	l.End = int(binary.BigEndian.Uint16(data[4:]))
}

func (l *Loop) write(data []byte) {
	binary.BigEndian.PutUint16(data, uint16(l.Mode))
	binary.BigEndian.PutUint16(data[2:], uint16(l.Begin))
	binary.BigEndian.PutUint16(data[4:], uint16(l.End))
}

// An Instrument is a chunk describing how to use the data in an AIFF file as a
// sampled musical instrument.
type Instrument struct {
	BaseNote     int
	Detune       int
	LowNote      int
	HighNote     int
	LowVelocity  int
	HighVelocity int
	Gain         int
	SustainLoop  Loop
	ReleaseLoop  Loop
}

func (c *Instrument) parseChunk(data []byte, _ bool) error {
	if len(data) < 20 {
		return errors.New("instrument chunk too short")
	}
	c.BaseNote = int(data[0])
	c.Detune = int(data[1])
	c.LowNote = int(data[2])
	c.HighNote = int(data[3])
	c.LowVelocity = int(data[4])
	c.HighVelocity = int(data[5])
	c.Gain = int(int16(binary.BigEndian.Uint16(data[6:])))
	c.SustainLoop.parse(data[8:])
	c.SustainLoop.parse(data[14:])
	return nil
}

// ChunkData implements the Chunk interface.
func (c *Instrument) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "INST")
	data = make([]byte, 20)
	data[0] = byte(c.BaseNote)
	data[1] = byte(c.Detune)
	data[2] = byte(c.LowNote)
	data[3] = byte(c.HighNote)
	data[4] = byte(c.LowVelocity)
	data[5] = byte(c.HighVelocity)
	binary.BigEndian.PutUint16(data[6:], uint16(c.Gain))
	c.SustainLoop.write(data[8:])
	c.SustainLoop.write(data[14:])
	return id, data, nil
}

// =============================================================================

// A ApplicationData is the APPL chunk in an AIFF file.
type ApplicationData struct {
	Signature [4]byte
	Data      []byte
}

// ChunkData implements the Chunk interface.
func (c *ApplicationData) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	copy(id[:], "APPL")
	data = make([]byte, 4+len(c.Data))
	copy(data, c.Signature[:])
	copy(data[4:], c.Data)
	return
}

type applChunk interface {
	Chunk
	parseAPPL(data []byte) error
}

func (c *ApplicationData) parseAPPL(data []byte) (Chunk, error) {
	if len(data) < 4 {
		return nil, errors.New("APPL chunk too short")
	}
	copy(c.Signature[:], data)
	d := make([]byte, len(data)-4)
	copy(d, data[4:])
	c.Data = d
	if string(c.Signature[:]) != "stoc" {
		return c, nil
	}
	if len(d) == 0 {
		return nil, errors.New("APPL chunk missing string")
	}
	n := int(d[0])
	d = d[1:]
	if n > len(d) {
		return nil, errors.New("APPL chunk missing string")
	}
	s := string(d[:n])
	d = d[n:]
	var ck applChunk
	switch s {
	case "VADPCMCODES":
		ck = new(VADPCMCodes)
	case "VADPCMLOOPS":
		ck = new(VADPCMLoops)
	default:
		return c, nil
	}
	if err := ck.parseAPPL(d); err != nil {
		return nil, err
	}
	return ck, nil
}

// writeStoc creates an APPL / stock chunk.
func writeStoc(name string, adata []byte) (id [4]byte, data []byte, err error) {
	copy(id[:], "APPL")
	hlen := (5 + len(name) + 1) &^ 1
	data = make([]byte, hlen+len(adata))
	copy(data[0:4], "stoc")
	data[4] = byte(len(name))
	copy(data[5:], name)
	copy(data[hlen:], adata)
	return id, data, nil
}

// =============================================================================

// A VADPCMCodes contains the codebook for a VADPCM-compressed file.
type VADPCMCodes struct {
	Version    int
	Order      int
	NumEntries int
	Table      []int16
}

const vadpcmVectorsize = 8

// ChunkData implements the Chunk interface.
func (c *VADPCMCodes) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	tsize := c.Order * c.NumEntries * vadpcmVectorsize
	if tsize != len(c.Table) {
		return id, data, fmt.Errorf("incorrect VADPCM table size: table has size %d, should be %d", len(c.Table), tsize)
	}
	adata := make([]byte, 6+2*tsize)
	binary.BigEndian.PutUint16(adata[0:], uint16(c.Version))
	binary.BigEndian.PutUint16(adata[2:], uint16(c.Order))
	binary.BigEndian.PutUint16(adata[4:], uint16(c.NumEntries))
	for i, x := range c.Table {
		binary.BigEndian.PutUint16(data[6+2*i:], uint16(x))
	}
	return writeStoc("VADPCMCODES", adata)
}

func (c *VADPCMCodes) parseAPPL(data []byte) error {
	if len(data) < 6 {
		return errors.New("VADPCMCodes too short")
	}
	c.Version = int(binary.BigEndian.Uint16(data[0:2]))
	c.Order = int(binary.BigEndian.Uint16(data[2:4]))
	c.NumEntries = int(binary.BigEndian.Uint16(data[4:6]))
	if c.Order < 1 || 16 < c.Order {
		return fmt.Errorf("order out of range: %d", c.Order)
	}
	if c.NumEntries < 1 || 1024 < c.NumEntries {
		return fmt.Errorf("num entries out of range: %d", c.NumEntries)
	}
	data = data[6:]
	tsize := c.Order * c.NumEntries * vadpcmVectorsize
	if len(data) != tsize*2 {
		return fmt.Errorf("table has %d bytes, expected %d", len(data), tsize*2)
	}
	table := make([]int16, tsize)
	for i := range table {
		table[i] = int16(binary.BigEndian.Uint16(data[i*2 : i*2+2]))
	}
	c.Table = table
	return nil
}

// =============================================================================

// VADPCMLoopSize is the size of a VADPCM loop record.
const VADPCMLoopSize = 44

// A VADPCMLoop is a loop in a VADPCM-encoded file.
type VADPCMLoop struct {
	Start int
	End   int
	Count int
	State [16]int16
}

// Marshal writes the loop to the given buffer.
func (l *VADPCMLoop) Marshal(d []byte) {
	binary.BigEndian.PutUint32(d, uint32(l.Start))
	binary.BigEndian.PutUint32(d[4:], uint32(l.End))
	binary.BigEndian.PutUint32(d[8:], uint32(l.Count))
	for i, x := range l.State {
		binary.BigEndian.PutUint16(d[12+2*i:], uint16(x))
	}
}

// Unmarshal reads the loop from the given buffer.
func (l *VADPCMLoop) Unmarshal(d []byte) {
	l.Start = int(binary.BigEndian.Uint32(d))
	l.End = int(binary.BigEndian.Uint32(d[4:]))
	l.Count = int(binary.BigEndian.Uint32(d[8:]))
	for i := range l.State {
		l.State[i] = int16(binary.BigEndian.Uint16(d[12+2*i:]))
	}
}

// A VADPCMLoops contains the loop information for a VADPCM-encoded file.
type VADPCMLoops struct {
	Loops []VADPCMLoop
}

// ChunkData implements the Chunk interface.
func (c *VADPCMLoops) ChunkData(_ bool) (id [4]byte, data []byte, err error) {
	adata := make([]byte, 4+VADPCMLoopSize*len(c.Loops))
	binary.BigEndian.PutUint16(adata, 1)
	binary.BigEndian.PutUint16(adata[2:], uint16(len(c.Loops)))
	d := adata[4:]
	for _, l := range c.Loops {
		l.Marshal(d)
		d = d[VADPCMLoopSize:]
	}
	return writeStoc("VADPCMLOOPS", adata)
}

func (c *VADPCMLoops) parseAPPL(data []byte) error {
	if len(data) < 2 {
		return errUnexpectedEOF
	}
	ver := binary.BigEndian.Uint16(data)
	if ver != 1 {
		return fmt.Errorf("unknown VADPCMLOOPS version: %d", ver)
	}
	d := data[2:]
	if len(d) < 2 {
		return errUnexpectedEOF
	}
	count := int(binary.BigEndian.Uint16(data))
	d = d[2:]
	if len(d) < VADPCMLoopSize*count {
		return errUnexpectedEOF
	}
	loops := make([]VADPCMLoop, count)
	for i := range loops {
		loops[i].Unmarshal(d)
		d = d[VADPCMLoopSize:]
	}
	c.Loops = loops
	return nil
}

// =============================================================================

var errUnexpectedEOF = errors.New("unexpected end of file in AIFF data")

// ErrNotAiff indicates that the file is not an AIFF file.
var ErrNotAIFF = errors.New("not an AIFF file")

// Parse an AIFF or AIFF-C file.
func Parse(data []byte) (*AIFF, error) {
	if len(data) < 12 {
		return nil, errors.New("AIFF too short")
	}
	header := data[0:12:12]
	if string(header[0:4]) != "FORM" {
		return nil, errors.New("not an AIFF file")
	}
	var compressed bool
	switch string(header[8:12]) {
	case "AIFF":
	case "AIFC":
		compressed = true
	default:
		return nil, ErrNotAIFF
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
		var ck parsableChunk
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
		case "MARK":
			ck = new(Markers)
		case "INST":
			ck = new(Instrument)
		case "APPL":
			d := new(ApplicationData)
			ck, err := d.parseAPPL(cdata)
			if err != nil {
				return nil, fmt.Errorf("could not parse %q chunk: %w", ch[:4], err)
			}
			chunks = append(chunks, ck)
			continue
		default:
			r := new(RawChunk)
			copy(r.ID[:], ch[:4])
			ck = r
		}
		if err := ck.parseChunk(cdata, compressed); err != nil {
			return nil, fmt.Errorf("could not parse %q chunk: %w", ch[:4], err)
		}
		chunks = append(chunks, ck)
	}
	if !hasCommon {
		return nil, errors.New("missing common chunk")
	}
	// if compressed && !hasFVer {
	// 	return nil, errors.New("missing FVER chunk")
	// }
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
		binary.BigEndian.PutUint32(data[:], StandardVersion)
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
