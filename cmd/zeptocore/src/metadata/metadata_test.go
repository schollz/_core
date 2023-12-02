package metadata

import (
	"testing"

	log "github.com/schollz/logger"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/zeptocore"
	"github.com/stretchr/testify/assert"
)

func TestMetadata(t *testing.T) {
	log.SetLevel("trace")
	err := SetMetadata("amen.0.wav", zeptocore.File{
		SliceStart: []float64{0.123, 0.234, 0.345, 0.456},
	})
	assert.Nil(t, err)

	metadata, err := GetMetadata("amen.0.wav")
	assert.Nil(t, err)
	assert.Equal(t, []float64{0.123, 0.234, 0.345, 0.456}, metadata.SliceStart)

	_, err = GetMetadata("amen.1.wav")
	assert.Equal(t, ErrNoMetadata, err)
}
