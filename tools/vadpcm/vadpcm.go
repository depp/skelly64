package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

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
		copy(vectors[i][:], ck.Table[i*8:i*8+1])
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
		if ext := filepath.Ext(fileout); !strings.EqualFold(ext, ".aiff") && !strings.EqualFold(ext, ".aifc") {
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
		var chunks []aiff.Chunk
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
			default:
				chunks = append(chunks, ck)
			}
		}

		// Decode.
		if a.Common.NumChannels != 1 {
			return &fileError{filein, fmt.Errorf("number of channels is %d, only 1-channel audio is supported", a.Common.NumChannels)}
		}
		vdata := a.Data.Data
		nvframes := a.Common.NumFrames / vadpcm.FrameSampleCount
		nframes := nvframes * vadpcm.FrameSampleCount
		if nframes != a.Common.NumFrames {
			logrus.Warnf("AIFF numSampleFrames is %d, which is not divisible by %d", a.Common.NumFrames, vadpcm.FrameSampleCount)
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
			Data: &aiff.SoundData{
				Data: odata,
			},
			Chunks: chunks,
		}
		data, err = o.Write(strings.EqualFold(filepath.Ext(fileout), ".aifc"))
		if err != nil {
			return err
		}
		return ioutil.WriteFile(fileout, data, 0666)
	},
}

func main() {
	cmdRoot.AddCommand(&cmdDecode)
	if err := cmdRoot.Execute(); err != nil {
		logrus.Error(err)
		os.Exit(1)
	}
}
