package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/depp/extended"
	"github.com/depp/skelly64/lib/audio/aiff"
	"github.com/depp/skelly64/lib/audio/metadata"
	"github.com/depp/skelly64/lib/getpath"
)

type options struct {
	input    string
	metadata string
	output   string
	rate     int
	flac     string
	sox      string
}

func getArgs() (o options, err error) {
	flag.StringVar(&o.input, "input", "", "input audio file")
	flag.StringVar(&o.metadata, "metadata", "", "input metadata JSON file")
	flag.StringVar(&o.output, "output", "", "output audio file")
	flag.IntVar(&o.rate, "rate", 0, "audio sample rate")
	flag.StringVar(&o.flac, "flac", "", "flac executable")
	flag.StringVar(&o.sox, "sox", "", "sox executable")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return o, fmt.Errorf("unexpected argumen: %q", args[0])
	}
	if o.input == "" {
		return o, errors.New("missing required flag -input")
	}
	o.input = getpath.GetPath(o.input)
	if o.metadata != "" {
		o.metadata = getpath.GetPath(o.metadata)
	}
	if o.output == "" {
		return o, errors.New("missing required flag -output")
	}
	o.output = getpath.GetPath(o.output)
	if o.rate == 0 {
		return o, errors.New("missing required flag -rate")
	}
	if o.flac != "" {
		o.flac, err = filepath.Abs(o.flac)
		if err != nil {
			return o, err
		}
	} else {
		o.flac = "flac"
	}
	if o.sox != "" {
		o.sox, err = filepath.Abs(o.sox)
		if err != nil {
			return o, err
		}
	} else {
		o.sox = "sox"
	}
	return o, nil
}

// pcmdata contais 16-bit mono PCM.
type pcmdata struct {
	rate      int
	markers   []aiff.Marker
	maxmarker int
	inst      *aiff.Instrument
	samples   []int16
}

// readPCM reads the input as 16-bit mono PCM.
func readPCM(opts *options) (*pcmdata, error) {
	pcmfile := opts.input

	// Decode FLAC.
	if strings.HasSuffix(opts.input, ".flac") {
		wav, err := ioutil.TempFile(filepath.Dir(opts.output), "audioconvert.*.wav")
		if err != nil {
			return nil, err
		}
		defer func() {
			wav.Close()
			os.Remove(wav.Name())
		}()
		cmd := exec.Command(opts.flac, "--decode", "--stdout", "--silent", opts.input)
		cmd.Stdout = wav
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return nil, fmt.Errorf("could not decode FLAC: %w", err)
		}
		pcmfile = wav.Name()
	}

	// Convert to the right format and rate.
	cmd := exec.Command(
		opts.sox,
		pcmfile,
		"--bits", "16",
		"--channels", "1",
		"--rate", strconv.Itoa(opts.rate),
		"--encoding", "signed-integer",
		"--type", "raw",
		"--endian", "little",
		"-",
	)
	if opts.sox != "" {
		cmd.Path = opts.sox
	}
	var buf bytes.Buffer
	cmd.Stdout = &buf
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("could not convert audio format: %w", err)
	}
	raw := buf.Bytes()
	r := make([]int16, len(raw)/2)
	for i := range r {
		r[i] = int16(binary.LittleEndian.Uint16(raw[i*2:]))
	}
	return &pcmdata{
		rate:    opts.rate,
		samples: r,
	}, nil
}

func (pcm *pcmdata) addMarker(pos int, name string) int {
	id := pcm.maxmarker + 1
	pcm.maxmarker = id + 1
	pcm.markers = append(pcm.markers, aiff.Marker{
		ID:       id,
		Position: pos,
		Name:     name,
	})
	return id
}

func alignSample(n int) int {
	return (n + 3) &^ 7
}

// makeLoop returns a seamless loop from the given PCM data according to the
// metadata.
func (pcm *pcmdata) makeLoop(md *metadata.Metadata) error {
	if md.LoopLength == nil {
		return nil
	}
	inlen := len(pcm.samples)
	looplen := alignSample(int(math.Round(*md.LoopLength * float64(pcm.rate))))
	extra := inlen - looplen
	info := fmt.Sprintf("loop = %f s, %d samples; audio = %f s, %d samples",
		float64(looplen)/float64(pcm.rate), looplen,
		float64(inlen)/float64(pcm.rate), inlen)
	if extra < 0 {
		return fmt.Errorf("loop is longer than audio (%s)", info)
	}
	if extra > inlen {
		return fmt.Errorf("loop is shorter than half of audio (%s)", info)
	}
	totallen := alignSample(inlen + extra)
	out := make([]int16, totallen)
	copy(out[looplen:], pcm.samples)
	for i, x := range pcm.samples {
		xx := int32(x) + int32(out[i])
		if xx < math.MinInt16 {
			xx = math.MinInt16
		} else if xx > math.MaxInt16 {
			xx = math.MaxInt16
		}
		out[i] = int16(xx)
	}
	pcm.samples = out
	begin := pcm.addMarker(totallen-looplen, "Begin")
	end := pcm.addMarker(totallen, "End")
	if pcm.inst == nil {
		pcm.inst = new(aiff.Instrument)
	}
	pcm.inst.SustainLoop = aiff.Loop{
		Mode:  aiff.LoopForward,
		Begin: begin,
		End:   end,
	}
	return nil
}

// writewrites the output as an AIFF-C file.
func (pcm *pcmdata) write(opts *options) error {
	data := make([]byte, 2*len(pcm.samples))
	for i, x := range pcm.samples {
		binary.BigEndian.PutUint16(data[i*2:], uint16(x))
	}
	a := aiff.AIFF{
		Common: aiff.Common{
			NumChannels:     1,
			NumFrames:       len(pcm.samples),
			SampleSize:      16,
			SampleRate:      extended.FromFloat64(float64(pcm.rate)),
			Compression:     [4]byte{'N', 'O', 'N', 'E'},
			CompressionName: "no compression",
		},
		Data: &aiff.SoundData{
			Data: data,
		},
	}
	a.Chunks = []aiff.Chunk{&a.Common}
	if len(pcm.markers) != 0 {
		a.Chunks = append(a.Chunks, &aiff.Markers{Markers: pcm.markers})
	}
	if pcm.inst != nil {
		a.Chunks = append(a.Chunks, pcm.inst)
	}
	a.Chunks = append(a.Chunks, a.Data)
	fdata, err := a.Write(true)
	if err != nil {
		return fmt.Errorf("could not create AIFF-C data: %v", err)
	}
	return ioutil.WriteFile(opts.output, fdata, 0666)
}

func mainE() error {
	opts, err := getArgs()
	if err != nil {
		return err
	}

	var md metadata.Metadata
	if opts.metadata != "" {
		md, err = metadata.Read(opts.metadata)
		if err != nil {
			return err
		}
	}

	pcm, err := readPCM(&opts)
	if err != nil {
		return err
	}

	if pos := int(math.Round(md.LeadIn * float64(opts.rate))); pos != 0 {
		pcm.addMarker(pos, "LeadIn")
	}

	if err := pcm.makeLoop(&md); err != nil {
		return err
	}

	return pcm.write(&opts)
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
