package zeptocore

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

// define error for no metadata present
var ErrNoMetadata = fmt.Errorf("no metadata present")

type Metadata struct {
	SliceStart []float64 `json:"s"`
	SliceStop  []float64 `json:"e"`
}

func SetMetadata(fname string, metadata Metadata) (err error) {
	if filepath.Ext(fname) != ".aif" {
		err = fmt.Errorf("only .aif files supported")
		return
	}
	// millisecond precision
	for i, v := range metadata.SliceStart {
		metadata.SliceStart[i] = toFixed(v, 5)
	}
	for i, v := range metadata.SliceStop {
		metadata.SliceStop[i] = toFixed(v, 5)
	}

	b, err := json.Marshal(metadata)
	if err != nil {
		log.Error(err)
		return
	}
	log.Tracef("setting metadata: %s", string(b))

	fname2, err := sox.AddComment(fname, string(b))
	if err != nil {
		log.Error(err)
		return
	}
	os.Rename(fname2, fname)

	return
}

func toFixed(num float64, precision int) float64 {
	output := math.Pow(10, float64(precision))
	return float64(math.Round(num*output)) / output
}

func GetMetadata(fname string) (metadata Metadata, err error) {
	data, err := sox.GetComment(fname)
	if err != nil {
		log.Error(err)
		return
	}
	err = json.Unmarshal([]byte(data), &metadata)
	if err != nil {
		return
	}
	if metadata.SliceStart == nil {
		err = ErrNoMetadata
		return
	}
	return
}
