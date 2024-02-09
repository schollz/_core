package kickextract

import (
	"testing"
)

func TestKickExtract(t *testing.T) {
	filename := "../../../lib/audio/amen_5c2d11c8_beats16_bpm170.flac"
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
	correctKicks := []bool{true, false, false, false, true, false, false, false, true, false, false, false, true, false, false, false}
	for i := 0; i < 16; i++ {
		if kicks[i] != correctKicks[i] {
			t.Errorf("kick %d: %t != %t", i, kicks[i], correctKicks[i])
		}
	}

}
