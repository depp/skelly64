package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"
	"os"

	"thornmarked/tools/audio"
)

// Check that the first n bytes of the data equal the magic, followed by nul
// bytes.
func checkMagic(data []byte, magic string, n int) error {
	if len(data) < n {
		return errors.New("file too short")
	}
	expect := make([]byte, n)
	dmagic := data[:n]
	if !bytes.Equal(dmagic[:len(magic)], []byte(magic)) {
		return fmt.Errorf("invalid header %q, expect %q", dmagic, expect)
	}
	return nil
}

// parseData parses an input data file into pak objects and stores the resulting
// chunks in the output.
func parseData(objects [][]byte, dtype datatype, data []byte) error {
	switch dtype {
	case typeData:
		if len(objects) != 1 {
			panic("wrong object length")
		}
		objects[0] = data
		return nil
	case typeFont:
		if len(objects) != 1 {
			panic("wrong object length")
		}
		if err := checkMagic(data, "Font", 16); err != nil {
			return err
		}
		objects[0] = data[16:]
		return nil
	case typeTrack:
		if len(objects) != 2 {
			panic("wrong object length")
		}
		tr, err := audio.ReadTrack(data)
		if err != nil {
			return err
		}
		objects[0] = tr.Codebook
		objects[1] = tr.Data
		return nil
	case typeModel:
		if len(objects) != 2 {
			panic("wrong object length")
		}
		if err := checkMagic(data, "Model", 16); err != nil {
			return err
		}
		if len(data) < 32 {
			return fmt.Errorf("model too small: %d bytes", len(data))
		}
		var hdr [4]uint32
		for i := 0; i < 4; i++ {
			hdr[i] = binary.BigEndian.Uint32(data[16+i*4 : 16+i*4+4])
		}
		objects[0] = data[hdr[0] : hdr[0]+hdr[1]]
		objects[1] = data[hdr[2] : hdr[2]+hdr[3]]
		return nil
	case typeTexture:
		if len(objects) != 1 {
			panic("wrong object length")
		}
		if err := checkMagic(data, "Texture", 16); err != nil {
			return err
		}
		objects[0] = data[16:]
		return nil
	default:
		panic("bad type")
	}
}

// readData reads the entry data for this section and uses it to fill in the
// given array of objects.
func (sec *section) readData(objects [][]byte) error {
	n := sec.dtype.slotCount()
	for i, e := range sec.Entries {
		data, err := ioutil.ReadFile(e.fullpath)
		if err != nil {
			return err
		}
		if err := parseData(objects[i*n:i*n+n:i*n+n], sec.dtype, data); err != nil {
			return fmt.Errorf("could not parse %s %q: %v", err)
		}
	}
	return nil
}

func (mn *manifest) writeStats(filename string, odata [][]byte, offsets []uint32, size int) error {
	fp, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer fp.Close()
	w := bufio.NewWriter(fp)
	if _, err := fmt.Fprint(w, "Sections:\n"); err != nil {
		return err
	}
	for _, sec := range mn.Sections {
		if _, err := fmt.Fprintf(w, "  %s:\n", sec.dtype.name()); err != nil {
			return err
		}
		n := sec.dtype.slotCount()
		sizes := make([]int, n)
		tsizes := make([]int, n)
		var tsize int
		for i := range sec.Entries {
			off := sec.Start - 1 + i*n
			edata := odata[off : off+n : off+n]
			for j, d := range edata {
				if len(d) > sizes[j] {
					sizes[j] = len(d)
				}
				tsizes[j] += len(d)
				tsize += len(d)
			}
		}
		if _, err := fmt.Fprintf(w, "    Max size: %d\n", sizes); err != nil {
			return err
		}
		if _, err := fmt.Fprintf(w, "    Total sizes: %d %d\n", tsizes, tsize); err != nil {
			return err
		}
		switch sec.dtype {
		case typeModel:
			var maxfsize uint32
			for i := range sec.Entries {
				fsize := binary.BigEndian.Uint32(odata[sec.Start-1+i*n][12:16])
				if fsize > maxfsize {
					maxfsize = fsize
				}
			}
			if _, err := fmt.Fprintf(w, "    Max frame size: %d\n", maxfsize); err != nil {
				return err
			}
		}
	}
	if err := w.Flush(); err != nil {
		return err
	}
	return fp.Close()
}

func (mn *manifest) writeData(filename, statsfile string) error {
	// Get object data for all assets.
	odata := make([][]byte, mn.Size)
	for _, sec := range mn.Sections {
		n := len(sec.Entries) * sec.dtype.slotCount()
		if err := sec.readData(odata[sec.Start : sec.Start+n : sec.Start+n]); err != nil {
			return err
		}
	}
	odata = odata[1:]

	// Calculate offset of each object.
	offsets := make([]uint32, len(odata))
	const (
		hsz     = 8 // Header entry size.
		maxSize = 64 * 1024 * 1024
	)
	pos := mn.Size * hsz // Position in pak file
	for i, data := range odata {
		pos = (pos + 1) &^ 1
		if len(data) > maxSize-pos {
			return errors.New("too much data in pak")
		}
		offsets[i] = uint32(pos)
		pos += len(data)
	}

	// Write stats.
	if statsfile != "" {
		if err := mn.writeStats(statsfile, odata, offsets, pos); err != nil {
			return fmt.Errorf("could not write stats: %v", err)
		}
	}

	// Write objects to buffer.
	pdata := make([]byte, pos)
	for i, data := range odata {
		offset := offsets[i]
		h := pdata[i*hsz : (i+1)*hsz : (i+1)*hsz]
		binary.BigEndian.PutUint32(h[0:4], offset)
		binary.BigEndian.PutUint32(h[4:8], uint32(len(data)))
		copy(pdata[offset:], data)
	}
	return ioutil.WriteFile(filename, pdata, 0666)
}
