package onsetdetect

import (
	"testing"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestOnsetDetect(t *testing.T) {
	log.SetLevel("trace")
	onsets, err := OnsetDetect("../sox/amen_beats8_bpm172.wav", 8)
	assert.Nil(t, err)
	assert.Equal(t, 8, len(onsets))
}
