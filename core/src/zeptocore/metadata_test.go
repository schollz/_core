package zeptocore

import (
	"testing"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestMetadata(t *testing.T) {
	log.SetLevel("trace")
	sox.Convert("1.wav", "1.aif")
	duration, err := sox.Length("1.aif")
	assert.Nil(t, err)
	err = SetMetadata("1.aif", Metadata{
		SliceStart: []float64{0.123, 0.234, 0.345, 0.456},
		SliceStop:  []float64{0.123, 0.234, 0.345, 0.456},
	})
	assert.Nil(t, err)

	metadata, err := GetMetadata("1.aif")
	assert.Nil(t, err)
	assert.Equal(t, []float64{0.123, 0.234, 0.345, 0.456}, metadata.SliceStart)

	duration2, err := sox.Length("1.aif")
	assert.Nil(t, err)
	assert.Equal(t, duration, duration2)
}
