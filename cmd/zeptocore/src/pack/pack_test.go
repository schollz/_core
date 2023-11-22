package pack

import (
	"testing"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestPack(t *testing.T) {
	log.SetLevel("trace")
	pathToStorage := "storage"
	payload := []byte(`{"oversampling":"1x","stereoMono":"stereo","banks":[{"files":["amen_0efedaab_beats8_bpm165.flac","amen_5c2d11c8_beats16_bpm170.flac","piano_beats8_bpm170.flac"]},{"files":["Sample00_160_yosemite_bpm160_beats16.flac"]},{"files":["Sample00_165_arts_sake_1_bpm165_beats16.flac","Sample00_165_arts_sake_2_bpm165_beats16.flac","Sample00_165_armfull_3_bpm165_beats16.flac"]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]},{"files":[]}]}`)
	_, err := Zip(pathToStorage, payload)
	assert.Nil(t, err)
}
