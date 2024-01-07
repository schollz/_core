package kickextract

import (
	"fmt"
	"testing"
)

func TestKickExtract(t *testing.T) {
	filename := "amen_5c2d11c8_beats16_bpm170.flac"
	slice_start := make([]float64, 16)
	slice_end := make([]float64, 16)
	for i := 0; i < 16; i++ {
		slice_start[i] = float64(i) / 16.0
		slice_end[i] = float64(i+1) / 16.0
	}
	kicks, err := KickExtract(filename, slice_start, slice_end)
	if err != nil {
		t.Error(err)
	}
	for i := 0; i < 16; i++ {
		fmt.Printf("kick %d: %t\n", i, kicks[i])
	}

}
