package op1

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestOP1(t *testing.T) {
	s, e, err := GetSliceMarkers("lofi.aif")
	assert.Equal(t, []int{0, 8680, 18071, 47810, 64804, 69763, 80802, 92358, 104136, 112647, 117646, 135143, 140701, 147233, 171710, 179193, 204735, 226335, 235540, 247021, 259676, 284323}, s)
	assert.Equal(t, []int{8639, 18030, 47769, 64763, 69722, 80761, 92317, 104095, 112606, 117605, 135102, 140660, 147192, 171669, 179152, 204694, 226294, 235499, 246980, 259635, 284282, 521317}, e)
	assert.Nil(t, err)
	for i, _ := range s {
		fmt.Printf("%d - %d\n", s[i], e[i])
	}
}
