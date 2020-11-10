package main

import (
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
)

func getPath(wd, filename string) string {
	if filename != "" && wd != "" && !filepath.IsAbs(filename) {
		return filepath.Join(wd, filename)
	}
	return filename
}

func mainE() error {
	wd := os.Getenv("BUILD_WORKING_DIRECTORY")
	// Parse flags.
	inFlag := flag.String("input", "", "input audio file")
	outFlag := flag.String("output", "", "output asset")
	rateFlag := flag.Int("rate", 0, "audio sample rate")
	flag.Parse()
	if args := flag.Args(); len(args) > 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	inpath := getPath(wd, *inFlag)
	if inpath == "" {
		return errors.New("missing -input flag")
	}
	outpath := getPath(wd, *outFlag)
	if outpath == "" {
		return errors.New("missing -output flag")
	}
	rate := *rateFlag
	if rate == 0 {
		return errors.New("missing -rate flag")
	}

	// Get the input as a PCM file.
	pcmfile := inpath
	switch ext := filepath.Ext(inpath); ext {
	case ".flac":
		fp, err := ioutil.TempFile(filepath.Dir(outpath), "audioconvert.*.wav")
		if err != nil {
			return err
		}
		defer func() {
			fp.Close()
			os.Remove(fp.Name())
		}()
		pcmfile = fp.Name()
		cmd := exec.Command("flac", "--decode", "--stdout", "--silent", inpath)
		cmd.Stdout = fp
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("could not decode FLAC: %w", err)
		}
		fp.Close()
	case ".wav", ".aif", ".aiff":
	default:
		return fmt.Errorf("input file has unknown extension: %q", ext)
	}

	// Convert to the right format and rate.
	cmd := exec.Command(
		"sox", pcmfile,
		"--bits", "16",
		"--channels", "1",
		"--rate", strconv.Itoa(rate),
		"--encoding", "signed-integer",
		"--type", "raw",
		"--endian", "big",
		"-",
	)
	fp, err := os.Create(outpath)
	if err != nil {
		return err
	}
	defer fp.Close()
	cmd.Stdout = fp
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("could not read WAV file: %w", err)
	}
	fp.Close()

	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
