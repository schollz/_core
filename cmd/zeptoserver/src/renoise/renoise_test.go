package renoise

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestUnzip(t *testing.T) {
	DebugMode = true
	filename, start, end, err := GetSliceMarkers("amen.xrni")
	assert.Nil(t, err)
	assert.Equal(t, "Sample00 (Amen Brother (Version 1) 1).flac", filename)
	assert.Equal(t, []int{0, 9403, 19561, 29757, 34833, 39376, 44021, 48488, 53694, 57783, 68060, 72827, 77613, 87215, 97040, 107191, 111672, 116746, 120879, 125783, 131073, 135262, 145403}, start)
	assert.Equal(t, []int{9403, 19561, 29757, 34833, 39376, 44021, 48488, 53694, 57783, 68060, 72827, 77613, 87215, 97040, 107191, 111672, 116746, 120879, 125783, 131073, 135262, 145403, 150270}, end)

	_, _, _, err = GetSliceMarkers("does not exist.xrni")
	assert.NotNil(t, err)
}
