package chapters

import (
	"testing"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestUnzip(t *testing.T) {
	log.SetLevel("trace")
	DebugMode = true
	start, end, err := GetSliceMarkers("morphagene.wav")
	assert.Nil(t, err)
	log.Tracef("start: %+v", start)
	log.Tracef("end: %+v", end)
}
