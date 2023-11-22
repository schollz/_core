package pack

import (
	"encoding/json"
	"fmt"

	"github.com/lucasepe/codename"

	log "github.com/schollz/logger"
)

type Data struct {
	Oversampling string `json:"oversampling"`
	StereoMono   string `json:"stereoMono"`
	Banks        []struct {
		Files []string `json:"files"`
	} `json:"banks"`
}

func Zip(pathToStorage string, payload []byte) (zipFilename string, err error) {
	// get all the files
	// each file is a folder inside pathToStorage
	var data Data
	err = json.Unmarshal(payload, &data)
	if err != nil {
		log.Error(err)
		return
	}
	log.Tracef("data: %+v", data)

	rng, err := codename.DefaultRNG()
	if err != nil {
		log.Error(err)
		return
	}

	for i := 0; i < 8; i++ {
		name := codename.Generate(rng, 0)
		fmt.Println(name)
	}
	return
}
