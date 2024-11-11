package op1

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math"
	"os"

	"github.com/anacrolix/log"
	"github.com/schollz/_core/core/src/sox"
)

func GetSliceMarkers(fname string) (spliceStart []float64, spliceEnd []float64, err error) {
	// catch panic
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("panic: %v", r)
			log.Error(err)
		}
	}()

	b, err := os.ReadFile(fname)
	if err != nil {
		return
	}
	datab := b[50:8000]
	ssnd := bytes.Index(datab, []byte("}"))

	var op1 OP1
	err = json.Unmarshal(datab[:ssnd+1], &op1)
	if err != nil {
		return
	}

	numSamples, err := sox.NumSamples(fname)
	if err != nil {
		return
	}
	lastStart := -1

	for i, _ := range op1.Start {
		start := int(math.Round(float64(op1.Start[i])/4058)) - 441
		if start < 0 {
			start = 0
		}
		if start == lastStart {
			continue
		}
		lastStart = start
		end := int(math.Round(float64(op1.End[i])/4058)) - 800
		if end > start {
			spliceStart = append(spliceStart, float64(start)/float64(numSamples))
			spliceEnd = append(spliceEnd, float64(end)/float64(numSamples))
		}
	}
	return
}

type OP1 struct {
	Start []int `json:"start"`
	End   []int `json:"end"`
}
