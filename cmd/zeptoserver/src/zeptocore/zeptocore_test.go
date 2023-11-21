package zeptocore

import (
	"fmt"
	"os"
	"testing"
	"time"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestOp1(t *testing.T) {
	log.SetLevel("trace")
	os.Remove("lofi.aif.json")
	f, err := Get("lofi.aif")
	assert.Nil(t, err)
	fmt.Println(f)
	f.SetBPM(120)
	time.Sleep(500 * time.Millisecond)
	f.SetOneshot(true)
	f.SetChannels(2)
	time.Sleep(5 * time.Second)
}
