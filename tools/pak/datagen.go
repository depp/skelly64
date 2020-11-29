package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io/ioutil"

	"thornmarked/tools/audio"
)

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

func getData(e *entry, data []byte) ([][]byte, error) {
	switch e.dtype {
	case typeData:
		return [][]byte{data}, nil
	case typeTrack:
		tr, err := audio.ReadTrack(data)
		if err != nil {
			return nil, err
		}
		return [][]byte{tr.Codebook, tr.Data}, nil
	case typeModel:
		if err := checkMagic(data, "Model", 16); err != nil {
			return nil, err
		}
		return [][]byte{data[16:]}, nil
	case typeTexture:
		if err := checkMagic(data, "Texture", 16); err != nil {
			return nil, err
		}
		return [][]byte{data[16:]}, nil
	default:
		panic("bad type")
	}
}

func writeData(filename string, mn *manifest) error {
	type dinfo struct {
		data []byte
		pos  int
	}
	var data []dinfo
	const (
		hsz     = 8
		maxSize = 128 * 1024 * 1024
	)
	pos := (mn.Size - 1) * hsz
	for _, e := range mn.Entries {
		if len(data)+1 != e.Index {
			panic("out of sync with pak index")
		}
		if e.fullpath == "" {
			return fmt.Errorf("no such file found: %q", e.filename)
		}
		odata, err := ioutil.ReadFile(e.fullpath)
		if err != nil {
			return fmt.Errorf("could not load %s: %w", e.filename, err)
		}
		chunks, err := getData(e, odata)
		if err != nil {
			return fmt.Errorf("could not parse %s: %w", e.filename, err)
		}
		for _, chunk := range chunks {
			pos = (pos + 1) &^ 1
			data = append(data, dinfo{chunk, pos})
			pos += len(chunk)
		}
		if pos > maxSize {
			return fmt.Errorf("data too large: %d bytes", pos)
		}
	}
	if len(data)+1 != mn.Size {
		panic("out of sync with pak index")
	}
	pdata := make([]byte, pos)
	for i, d := range data {
		h := pdata[i*hsz : (i+1)*hsz : (i+1)*hsz]
		binary.BigEndian.PutUint32(h[0:4], uint32(d.pos))
		binary.BigEndian.PutUint32(h[4:8], uint32(len(d.data)))
		copy(pdata[d.pos:], d.data)
	}
	return ioutil.WriteFile(filename, pdata, 0666)
}
