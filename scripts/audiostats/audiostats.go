package main

import (
	"encoding/csv"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
)

var sampleRates = []int{
	11025,
	16000,
	22050,
	32000,
}

const (
	vadpcmPath   = "tools/vadpcm/vadpcm"
	originalPath = "original"
	pcmPath      = "raw"
)

var (
	rootDir    string
	soxPath    string
	ffmpegPath string
)

type fileinfo struct {
	name string
	src  string
}

func listFiles(dir string) ([]fileinfo, error) {
	original := filepath.Join(dir, "original")
	fs, err := os.ReadDir(original)
	if err != nil {
		return nil, err
	}
	var r []fileinfo
	for _, f := range fs {
		name := f.Name()
		if strings.HasPrefix(name, ".") {
			continue
		}
		i := strings.LastIndexByte(name, '.')
		if i == -1 {
			continue
		}
		ext := name[i:]
		if ext == ".txt" {
			continue
		}
		base := name[:i]
		r = append(r, fileinfo{name: base, src: name})
	}
	return r, nil
}

type stats struct {
	SignalLevel float64
	ErrorLevel  float64
}

func processFile(fi fileinfo) ([]float64, error) {
	original := filepath.Join(rootDir, originalPath, fi.src)
	dir := filepath.Join(rootDir, pcmPath)
	pcmMaster := filepath.Join(dir, fi.name+".aiff")
	if err := os.Remove(pcmMaster); err != nil && !os.IsNotExist(err) {
		return nil, err
	}
	cmd := exec.Command(ffmpegPath,
		"-hide_banner", "-loglevel", "error",
		"-i", original, pcmMaster)
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return nil, err
	}

	snrs := make([]float64, len(sampleRates))
	for i, rate := range sampleRates {
		dir := filepath.Join(rootDir, "rate_"+strconv.Itoa(rate))
		pcm1 := filepath.Join(dir, fi.name+".in.aiff")
		vadpcm := filepath.Join(dir, fi.name+".vadpcm.aifc")
		pcm2 := filepath.Join(dir, fi.name+".out.aiff")
		statsPath := filepath.Join(dir, fi.name+".json")
		for _, p := range [...]string{pcm1, vadpcm, pcm2} {
			if err := os.Remove(p); err != nil && !os.IsNotExist(err) {
				return nil, err
			}
		}
		cmd := exec.Command(soxPath,
			pcmMaster,
			"-V1",
			"--channels=1",
			"--bits=16",
			"--rate="+strconv.Itoa(rate),
			pcm1)
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return nil, err
		}
		cmd = exec.Command(vadpcmPath,
			"encode", "-stats-out="+statsPath, pcm1, vadpcm)
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return nil, err
		}
		cmd = exec.Command(vadpcmPath,
			"decode", vadpcm, pcm2)
		cmd.Stderr = os.Stderr
		if err := cmd.Run(); err != nil {
			return nil, err
		}
		data, err := ioutil.ReadFile(statsPath)
		if err != nil {
			return nil, err
		}
		var st stats
		if err := json.Unmarshal(data, &st); err != nil {
			return nil, fmt.Errorf("invalid stats: %s: %v", statsPath, err)
		}
		if st.SignalLevel == 0 || st.ErrorLevel == 0 {
			return nil, fmt.Errorf("invalid stats: %s", statsPath)
		}
		snrs[i] = st.ErrorLevel / st.SignalLevel
	}

	return snrs, nil
}

func writeCSV(outPath string, files []fileinfo, snrs [][]float64) error {
	fp, err := os.Create(outPath)
	if err != nil {
		return err
	}
	defer fp.Close()
	w := csv.NewWriter(fp)
	row := make([]string, len(sampleRates)+1)
	row[0] = "File"
	for i, r := range sampleRates {
		row[i+1] = strconv.Itoa(r)
	}
	if err := w.Write(row); err != nil {
		return err
	}
	for i, fi := range files {
		row[0] = fi.name
		for j, s := range snrs[i] {
			row[j+1] = strconv.FormatFloat(s, 'e', 7, 64)
		}
		if err := w.Write(row); err != nil {
			return err
		}
	}
	w.Flush()
	if err := w.Error(); err != nil {
		return err
	}
	return fp.Close()
}

func mainE() error {
	flag.Parse()
	args := flag.Args()
	if len(args) < 1 || 2 < len(args) {
		fmt.Fprintln(os.Stderr, "Usage: audiostats <dir> [<out>]")
		return nil
	}
	rootDir = args[0]
	var outPath string
	if len(args) >= 2 {
		outPath = args[1]
	}

	// Find programs.
	var err error
	soxPath, err = exec.LookPath("sox")
	if err != nil {
		return err
	}
	ffmpegPath, err = exec.LookPath("ffmpeg")
	if err != nil {
		return err
	}

	// Get list of input files.
	files, err := listFiles(rootDir)
	if err != nil {
		return err
	}
	if len(files) == 0 {
		return errors.New("found no files")
	}

	// Create directories.
	if err := os.Mkdir(filepath.Join(rootDir, pcmPath), 0777); err != nil && !os.IsExist(err) {
		return err
	}
	for _, rate := range sampleRates {
		if err := os.Mkdir(filepath.Join(rootDir, "rate_"+strconv.Itoa(rate)), 0777); err != nil && !os.IsExist(err) {
			return err
		}
	}

	var wg sync.WaitGroup
	n := runtime.NumCPU()
	wg.Add(n)
	var pos, nerrors uint32
	snrs := make([][]float64, len(files))
	for i := 0; i < n; i++ {
		go func() {
			defer wg.Done()
			for {
				i := int(atomic.AddUint32(&pos, 1) - 1)
				if i >= len(files) {
					break
				}
				snr, err := processFile(files[i])
				if err != nil {
					fmt.Fprintln(os.Stderr, "Error:", err)
					atomic.AddUint32(&nerrors, 1)
					continue
				}
				snrs[i] = snr
			}
		}()
	}
	wg.Wait()
	if n := atomic.LoadUint32(&nerrors); n > 0 {
		return fmt.Errorf("%d errors occurred during processing", n)
	}

	if outPath != "" {
		if err := writeCSV(outPath, files, snrs); err != nil {
			return err
		}
	}

	var sum float64
	var count int
	for _, ss := range snrs {
		for _, s := range ss {
			sum += s
			count++
		}
	}
	snr := sum / float64(count)
	fmt.Printf("Average SNR: %.2f dB\n", -20.0*math.Log10(snr))

	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
