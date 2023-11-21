package zeptocore

import (
	"fmt"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestOp1(t *testing.T) {
	f, err := Get("lofi.aif")
	assert.Nil(t, err)
	fmt.Println(f)
	f.SetBPM(120)
	time.Sleep(1 * time.Second)
}
