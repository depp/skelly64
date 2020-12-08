package aiff

import (
	"encoding/binary"
	"math"
)

// Float80 returns a floating-point number converted to 80-bit extended big-endian.
func Float80(f float64) (d [10]byte) {
	i := math.Float64bits(f)
	sign := uint32(i >> 63)
	exp := uint32(i>>52) & ((1 << 11) - 1)
	frac := i & ((1 << 52) - 1)
	eexp := (sign << 15) | (exp + 16383 - 1023)
	efrac := (1 << 63) | (frac << 11)
	binary.BigEndian.PutUint16(d[:], uint16(eexp))
	binary.BigEndian.PutUint64(d[2:], efrac)
	return
}
