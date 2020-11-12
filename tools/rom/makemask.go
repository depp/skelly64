package main

import (
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"thornmarked/tools/getpath"
)

const (
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

func mainE() error {
	romArg := flag.String("rom", "", "path to ROM file")
	bootcodeArg := flag.String("bootcode", "", "path to boot code file")
	flag.Parse()
	if args := flag.Args(); len(args) != 0 {
		return fmt.Errorf("unexpected argument: %q", args[0])
	}
	romfile := getpath.GetPath(*romArg)
	if romfile == "" {
		return errors.New("missing required flag -rom")
	}
	bootcodeFile := getpath.GetPath(*bootcodeArg)
	if bootcodeFile == "" {
		return errors.New("missing required flag -bootcode")
	}

	// Get the boot code.
	bootcode, err := ioutil.ReadFile(bootcodeFile)
	if err != nil {
		return err
	}
	const maxBoot = loadOffset - bootOffset
	if len(bootcode) > maxBoot {
		return fmt.Errorf("boot code too long: %d bytes, should be %d bytes or smaller",
			len(bootcode), maxBoot)
	}

	// Resize the file and add the boot code.
	fp, err := os.OpenFile(romfile, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	st, err := fp.Stat()
	if err != nil {
		return err
	}
	sz := st.Size()
	const minSize = loadOffset + loadSize
	if sz < minSize {
		if err := fp.Truncate(minSize); err != nil {
			return err
		}
	}
	if _, err := fp.WriteAt(bootcode, bootOffset); err != nil {
		return err
	}

	// Calculate and update the checksums.
	data := make([]byte, loadSize)
	if _, err := fp.ReadAt(data, loadOffset); err != nil {
		return err
	}
	cksum := checksum(data)
	if _, err := fp.WriteAt(cksum[:], 0x10); err != nil {
		return err
	}

	return nil
}

func main() {
	if err := mainE(); err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}
}
