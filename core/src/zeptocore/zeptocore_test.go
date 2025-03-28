package zeptocore

import (
	"fmt"
	"os"
	"testing"
	"time"

	log "github.com/schollz/logger"
	"github.com/stretchr/testify/assert"
)

func TestOne(t *testing.T) {
	log.SetLevel("trace")
	os.Remove("lofi.aif.json")
	os.Remove("lofi.0.wav.info")
	os.Remove("lofi.0.wav")
	f, err := Get("lofi.aif")
	assert.Nil(t, err)
	fmt.Println(f)
	time.Sleep(2 * time.Second)
}

func TestOp1(t *testing.T) {
	log.SetLevel("trace")
	os.Remove("lofi.aif.json")
	os.Remove("lofi.0.wav")
	f, err := Get("lofi.aif")
	assert.Nil(t, err)
	fmt.Println(f)
	f.SetBPM(120)
	time.Sleep(500 * time.Millisecond)
	f.SetOneshot(true)
	f.SetChannels(1)
	time.Sleep(5 * time.Second)
}

func TestRenoise(t *testing.T) {
	log.SetLevel("trace")
	os.Remove("amen.xrni.json")
	os.Remove("amen.0.wav")
	f, err := Get("amen.xrni")
	assert.Nil(t, err)
	fmt.Printf("%+v\n", f)
	time.Sleep(500 * time.Millisecond)
	f.SetOneshot(false)
	f.SetChannels(0)
	time.Sleep(5 * time.Second)
}
