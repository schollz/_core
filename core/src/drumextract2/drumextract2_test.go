package drumextract2

import (
	"testing"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestDrumExtract(t *testing.T) {
	log.SetLevel("trace")
	err := DownloadModel()
	assert.Nil(t, err)

	kickTransients, snareTransients, err := DrumExtract2("../sox/amen_beats8_bpm172.wav")
	assert.Nil(t, err)
	assert.Equal(t, 16, len(kickTransients))
	assert.Equal(t, 16, len(snareTransients))
}
