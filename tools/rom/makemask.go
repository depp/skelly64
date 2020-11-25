package main

import (
	"debug/elf"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"thornmarked/tools/getpath"
)

const (
	maxSize    = 64 * 1024 * 1024
	bootOffset = 0x40
	loadOffset = 0x1000
	loadSize   = 1024 * 1024
)

func rol(num uint32, shift uint32) uint32 {
	return (num << shift) | (num >> (32 - shift))
}

func checksum(data []byte) (cksum [8]byte) {
	const seed uint32 = 0xF8CA4DDC
	t1 := seed
	t2 := seed
	t3 := seed
	t4 := seed
	t5 := seed
	t6 := seed
	for i := 0; i < loadSize; i += 4 {
		d := binary.BigEndian.Uint32(data[i:])
		if (t6 + d) < t6 {
			t4++
		}
		t6 += d
		t3 ^= d
		r := rol(d, (d & 0x1F))
		t5 += r
		if t2 > d {
			t2 ^= r
		} else {
			t2 ^= t6 ^ d
		}
		t1 += t5 ^ d
	}
	binary.BigEndian.PutUint32(cksum[0:], t6^t4^t3)
	binary.BigEndian.PutUint32(cksum[4:], t5^t2^t1)
	return
}

type options struct {
	output   string
	bootcode string
	program  string
	pak      string
}

func parseArgs() (opts options, err error) {
	output := flag.String("output", "", "path to output ROM file")
	bootcode := flag.String("bootcode", "", "path to boot code file")
	program := flag.String("program", "", "path to input ELF program")
	pak := flag.String("pak", "", "path to input pak file data")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return opts, fmt.Errorf("unexpected argument: %q", args[0])
	}
	opts.output = getpath.GetPath(*output)
	if opts.output == "" {
		return opts, errors.New("missing required flag -output")
	}
	opts.bootcode = getpath.GetPath(*bootcode)
	if opts.bootcode == "" {
		return opts, errors.New("missing required flag -bootcode")
	}
	opts.program = getpath.GetPath(*program)
	if opts.program == "" {
		return opts, errors.New("missing required flag -program")
	}
	opts.pak = getpath.GetPath(*pak)
	return opts, nil
}

func readBootcode(opts *options) ([]byte, error) {
	data, err := ioutil.ReadFile(opts.bootcode)
	if err != nil {
		return nil, err
	}
	const maxBoot = loadOffset - bootOffset
	if len(data) > maxBoot {
		return nil, fmt.Errorf("boot code too long: %d bytes, should be %d bytes or smaller",
			len(data), maxBoot)
	}
	return data, nil
}

func readProgram(opts *options) ([]byte, error) {
	f, err := elf.Open(opts.program)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	if f.Class != elf.ELFCLASS32 {
		return nil, fmt.Errorf("program ELF class is %v, expected %v", f.Class, elf.ELFCLASS32)
	}
	if f.ByteOrder != binary.BigEndian {
		return nil, errors.New("program byte order is not big-endian")
	}
	if f.Type != elf.ET_EXEC {
		return nil, fmt.Errorf("program ELF type is %v, expected %v", f.Type, elf.ET_EXEC)
	}
	if f.Machine != elf.EM_MIPS {
		return nil, fmt.Errorf("program machine is %v, expect %v", f.Machine, elf.EM_MIPS)
	}
	var romsize uint32
	for _, p := range f.Progs {
		if p.Filesz > 0 {
			end := uint32(p.Paddr + p.Filesz)
			if end > romsize {
				romsize = end
			}
		}
	}
	if romsize == 0 {
		return nil, errors.New("no ROM data")
	}
	if romsize > maxSize {
		return nil, fmt.Errorf("ROM too large: %d bytes, cannot be larger than %d", romsize, maxSize)
	}
	syms, err := f.Symbols()
	var start, pakdata uint32
	symNames := map[string]*uint32{
		"_start":          &start,
		"_pakdata_offset": &pakdata,
	}
	for _, sym := range syms {
		val := symNames[sym.Name]
		if val != nil {
			*val = uint32(sym.Value)
		}
	}
	const expectStart = 0x80000400
	if start != 0x80000400 {
		return nil, fmt.Errorf("_start has address 0x%08x, expect 0x%08x", start, expectStart)
	}
	if pakdata != 0 {
		if pakdata != romsize {
			return nil, fmt.Errorf("_pakdata_offset is not at ROM end")
		}
		if pakdata&1 != 0 {
			return nil, fmt.Errorf("_pakdata_offset is odd")
		}
	}
	data := make([]byte, romsize)
	for _, p := range f.Progs {
		if p.Filesz > 0 {
			if _, err := p.ReadAt(data[p.Paddr:p.Paddr+p.Filesz], 0); err != nil {
				return nil, err
			}
		}
	}
	return data, nil
}

func mainE() error {
	opts, err := parseArgs()
	if err != nil {
		return err
	}

	// Get the boot code.
	bootcode, err := readBootcode(&opts)
	if err != nil {
		return err
	}

	// Read the program.
	prog, err := readProgram(&opts)
	if err != nil {
		return err
	}

	// Read pak data.
	var pakdata []byte
	if opts.pak != "" {
		pakdata, err = ioutil.ReadFile(opts.pak)
		if err != nil {
			return err
		}
	}

	// Create the final ROM image.
	outlen := len(prog) + len(pakdata)
	const minSize = loadOffset + loadSize
	if outlen < minSize {
		outlen = minSize
	}
	data := make([]byte, outlen)
	copy(data, prog)
	copy(data[len(prog):], pakdata)
	copy(data[bootOffset:], bootcode)
	cksum := checksum(data[loadOffset : loadOffset+loadSize])
	copy(data[0x10:], cksum[:])

	return ioutil.WriteFile(opts.output, data, 0666)
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
