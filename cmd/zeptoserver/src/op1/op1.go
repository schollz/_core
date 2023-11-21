package op1

import (
	"bytes"
	"encoding/json"
	"math"
	"os"

	"github.com/schollz/zeptocore/cmd/zeptoserver/src/sox"
)

func GetSliceMarkers(fname string) (spliceStart []float64, spliceEnd []float64, err error) {
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
		start := int(math.Round(float64(op1.Start[i]) / 4058))
		if start == lastStart {
			continue
		}
		lastStart = start
		end := int(math.Round(float64(op1.End[i])/4058)) - 40
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
