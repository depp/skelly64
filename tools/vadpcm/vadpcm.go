package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"github.com/depp/extended"
	"github.com/depp/skelly64/lib/audio/aiff"
	"github.com/depp/skelly64/lib/audio/vadpcm"

	"github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
)

type fileError struct {
	name string
	err  error
}

func (e *fileError) Error() string {
	return fmt.Sprintf("%q: %v", e.name, e.err)
}

var cmdRoot = cobra.Command{
	Use:           "vadpcm",
	Short:         "VADPCM is a tool for encoding and decoding VADPCM audio.",
	SilenceErrors: true,
	SilenceUsage:  true,
}

func parseCodebook(ck *aiff.VADPCMCodes) (*vadpcm.Codebook, error) {
	if ck.Order < 0 || vadpcm.MaxOrder < ck.Order {
		return nil, fmt.Errorf("invalid predictor order: %d", ck.Order)
	}
	if ck.NumEntries < 0 {
		return nil, fmt.Errorf("invalid number of predictors: %d", ck.NumEntries)
	}
	sz := ck.Order * ck.NumEntries
	vectors := make([]vadpcm.Vector, sz)
	for i := range vectors {
		copy(vectors[i][:], ck.Table[i*vadpcm.VectorSize:i*vadpcm.VectorSize+vadpcm.VectorSize])
	}
	return &vadpcm.Codebook{
		Order:          ck.Order,
		PredictorCount: ck.NumEntries,
		Vectors:        vectors,
	}, nil
}

var cmdDecode = cobra.Command{
	Use:   "decode <input.aifc> <output.aiff>",
	Short: "Decode VADPCM-encoded AIFF files.",
	Args:  cobra.ExactArgs(2),
	RunE: func(_ *cobra.Command, args []string) error {
		filein := args[0]
		fileout := args[1]
		if ext := filepath.Ext(filein); !strings.EqualFold(ext, ".aifc") {
			logrus.Warn("input file does not have .aifc extension")
		}
		var kind aiff.Kind
		switch ext := filepath.Ext(fileout); {
		case strings.EqualFold(ext, ".aiff"):
			kind = aiff.AIFFKind
		case strings.EqualFold(ext, ".aifc"):
			kind = aiff.AIFCKind
		default:
			return fmt.Errorf("output file has unknown extension: %q", ext)
		}

		// Read input.
		data, err := ioutil.ReadFile(filein)
		if err != nil {
			return err
		}
		a, err := aiff.Parse(data)
		if err != nil {
			return &fileError{filein, err}
		}
		if string(a.Common.Compression[:]) != vadpcm.CompressionType {
			return &fileError{filein,
				fmt.Errorf("file does not contain VADPCM audio (compression type is %q, instead of %q)",
					string(a.Common.Compression[:]), vadpcm.CompressionType)}
		}
		var codebook *vadpcm.Codebook
		var sdata *aiff.SoundData
		for _, ck := range a.Chunks {
			switch ck := ck.(type) {
			case *aiff.VADPCMCodes:
				if codebook != nil {
					return &fileError{filein, errors.New("file contains multiple VADPCM codebooks")}
				}
				codebook, err = parseCodebook(ck)
				if err != nil {
					return &fileError{filein, fmt.Errorf("invalid VADPCM codebook: %v", err)}
				}
			case *aiff.SoundData:
				if sdata != nil {
					return &fileError{filein, errors.New("file contains multiple SSND chunks")}
				}
				sdata = ck
			}
		}

		// Decode.
		if a.Common.NumChannels != 1 {
			return &fileError{filein, fmt.Errorf("number of channels is %d, only 1-channel audio is supported", a.Common.NumChannels)}
		}
		nvframes := a.Common.NumFrames / vadpcm.FrameSampleCount
		nframes := nvframes * vadpcm.FrameSampleCount
		if nframes != a.Common.NumFrames {
			logrus.Warnf("AIFF numSampleFrames is %d, which is not divisible by %d", a.Common.NumFrames, vadpcm.FrameSampleCount)
		}
		var vdata []byte
		if nvframes > 0 {
			if sdata == nil {
				return &fileError{filein, errors.New("no SSND chunk")}
			}
			vdata = sdata.Data
			if len(vdata) < nvframes*vadpcm.FrameByteSize {
				return &fileError{filein, errors.New("SSND chunk is too short")}
			}
		}
		pdata := make([]int16, nframes)
		var state vadpcm.Vector
		if _, err := vadpcm.Decode(codebook, &state, pdata, vdata); err != nil {
			return &fileError{filein, err}
		}
		odata := make([]byte, nframes*2)
		for i, x := range pdata {
			binary.BigEndian.PutUint16(odata[i*2:i*2+2], uint16(x))
		}

		// Write output.
		o := aiff.AIFF{
			Common: aiff.Common{
				NumChannels:     1,
				NumFrames:       nframes,
				SampleSize:      16,
				SampleRate:      a.Common.SampleRate,
				Compression:     *(*[4]byte)([]byte(aiff.PCMType)),
				CompressionName: aiff.PCMName,
			},
			FormatVersion: aiff.FormatVersion{
				aiff.StandardVersion,
			},
			Chunks: []aiff.Chunk{
				&aiff.SoundData{Data: odata},
			},
		}
		data, err = o.Write(kind)
		if err != nil {
			return err
		}
		return ioutil.WriteFile(fileout, data, 0666)
	},
}

type audioData struct {
	rate    extended.Extended
	samples []int16
}

func readAudio(name string) (ad audioData, err error) {
	switch ext := filepath.Ext(name); {
	case strings.EqualFold(ext, ".aiff"):
	case strings.EqualFold(ext, ".aifc"):
	default:
		return ad, fmt.Errorf("input file has unknown extension: %q", ext)
	}
	data, err := ioutil.ReadFile(name)
	if err != nil {
		return ad, err
	}
	a, err := aiff.Parse(data)
	if err != nil {
		return ad, &fileError{name, err}
	}
	if ad.samples, err = a.GetSamples16(); err != nil {
		return ad, &fileError{name, err}
	}
	ad.rate = a.Common.SampleRate
	return ad, nil
}

func makeCodebookChunk(codebook *vadpcm.Codebook) *aiff.VADPCMCodes {
	table := make([]int16, vadpcm.VectorSize*codebook.Order*codebook.PredictorCount)
	for i, v := range codebook.Vectors {
		copy(table[i*vadpcm.VectorSize:i*vadpcm.VectorSize+vadpcm.VectorSize], v[:])
	}
	return &aiff.VADPCMCodes{
		Version:    1,
		Order:      codebook.Order,
		NumEntries: codebook.PredictorCount,
		Table:      table,
	}
}

var flagPredictorCount int

var cmdEncode = cobra.Command{
	Use:   "encode <input> <output.aifc>",
	Short: "Encode an audio file using VADPCM.",
	Args:  cobra.ExactArgs(2),
	RunE: func(_ *cobra.Command, args []string) error {
		filein := args[0]
		fileout := args[1]
		if ext := filepath.Ext(fileout); !strings.EqualFold(ext, ".aifc") {
			logrus.Warn("output file does not have .aifc extension")
		}
		if flagPredictorCount < 1 || vadpcm.MaxPredictorCount < flagPredictorCount {
			return fmt.Errorf("parameter count is not in the range 1-16: %d", flagPredictorCount)
		}

		// Encode.
		ad, err := readAudio(filein)
		if err != nil {
			return err
		}
		nvframes := (len(ad.samples) + vadpcm.FrameSampleCount - 1) / vadpcm.FrameSampleCount
		nframes := nvframes * vadpcm.FrameSampleCount
		if len(ad.samples) < nframes {
			ad.samples = append(ad.samples, make([]int16, nframes-len(ad.samples))...)
		}
		codebook, vdata, err := vadpcm.Encode(&vadpcm.Parameters{
			PredictorCount: flagPredictorCount,
		}, ad.samples)
		if err != nil {
			return err
		}
		o := aiff.AIFF{
			Common: aiff.Common{
				NumChannels:     1,
				NumFrames:       nframes,
				SampleSize:      16,
				SampleRate:      ad.rate,
				Compression:     *(*[4]byte)([]byte(vadpcm.CompressionType)),
				CompressionName: vadpcm.CompressionName,
			},
			FormatVersion: aiff.FormatVersion{
				aiff.StandardVersion,
			},
			Chunks: []aiff.Chunk{
				makeCodebookChunk(codebook),
				&aiff.SoundData{Data: vdata},
			},
		}
		data, err := o.Write(aiff.AIFCKind)
		if err != nil {
			return err
		}
		return ioutil.WriteFile(fileout, data, 0666)
	},
}

func main() {
	cmdRoot.AddCommand(&cmdDecode, &cmdEncode)
	f := cmdEncode.Flags()
	f.IntVar(&flagPredictorCount, "predictor-count", 4,
		"number of VADPCM predictors, 1-16")
	if err := cmdRoot.Execute(); err != nil {
		logrus.Error(err)
		os.Exit(1)
	}
}
