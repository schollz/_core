package drumextract2

import (
	"encoding/json"
	"os"
	"testing"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestDrumExtract(t *testing.T) {
	log.SetLevel("trace")
	err := DownloadModel()
	assert.Nil(t, err)

	filename := "../sox/amen_beats8_bpm172.wav"
	filename = "../../../dev/starting_samples_pikocore/default_sorted/bank0/amen_5c2d11c8_beats16_bpm170.flac"
	filename = "../../../dev/starting_samples_pikocore/default_sorted/bank0/Sample00_160_Lopus_Break_bpm160_beats8.flac"
	filename = "Magnetic_Break_171_PL__beats8_bpm171.wav"
	kickTransients, snareTransients, otherTransients, allTransients, err := drumExtract2(filename)
	assert.Nil(t, err)
	b, _ := json.Marshal(struct {
		Filename        string
		KickTransients  []int
		SnareTransients []int
		OtherTransients []int
		AllTransients   []int
	}{filename, kickTransients, snareTransients, otherTransients, allTransients})
	os.WriteFile("transients.json", b, 0644)
}
