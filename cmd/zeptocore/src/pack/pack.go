package pack

import (
	"encoding/json"
	"fmt"
	"os"
	"path"
	"time"

	"github.com/lucasepe/codename"

	log "github.com/schollz/logger"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/utils"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/zeptocore"
)

var Storage = "zips"

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

	oversampling := 1
	if data.Oversampling == "2x" {
		oversampling = 2
	} else if data.Oversampling == "4x" {
		oversampling = 4
	}

	channels := 1
	if data.StereoMono == "stereo" {
		channels = 2
	}

	rng, err := codename.DefaultRNG()
	if err != nil {
		log.Error(err)
		return
	}

	zipFilename = codename.Generate(rng, 0)

	// create a temporary folder to store the files
	err = os.MkdirAll(path.Join(Storage, zipFilename), 0777)
	if err != nil {
		return
	}

	// create the bank folders
	for i, bank := range data.Banks {
		log.Tracef("bank %d has %d files", i, len(bank.Files))
		if len(bank.Files) == 0 {
			continue
		}
		// process each file according to parameters
		for _, file := range bank.Files {
			log.Tracef("bank %d: %s", i, file)
			// get the file information
			var f zeptocore.File
			f, err = zeptocore.Get(path.Join(pathToStorage, file, file))
			if err != nil {
				log.Error(err)
				return
			}
			f.SetOversampling(oversampling)
			f.SetChannels(channels)
		}
	}
	time.Sleep(2 * time.Second)

	// wait until all the files are processed
	for i := 0; i < 300; i++ {
		time.Sleep(100 * time.Millisecond)
		if !zeptocore.IsBusy() {
			break
		}
	}
	if zeptocore.IsBusy() {
		err = fmt.Errorf("could not process all files")
		log.Error(err)
		return
	}

	// copy files
	for i, bank := range data.Banks {
		log.Tracef("bank %d has %d files", i, len(bank.Files))
		if len(bank.Files) == 0 {
			continue
		}
		bankFolder := path.Join(Storage, zipFilename, fmt.Sprintf("bank%d", i))
		err = os.MkdirAll(bankFolder, 0777)
		if err != nil {
			return
		}
		// go through each file and copy it into the bank
		for _, file := range bank.Files {
			log.Tracef("bank %d: %s", i, file)
			filenameWithoutExtension := file[:len(file)-len(path.Ext(file))]
			for i := 0; i < 100; i++ {
				oldFname := path.Join(pathToStorage, file, fmt.Sprintf("%s.%d.wav", filenameWithoutExtension, i))
				newFname := path.Join(bankFolder, fmt.Sprintf("%s.%d.wav", filenameWithoutExtension, i))
				if _, err := os.Stat(oldFname); os.IsNotExist(err) {
					break
				}
				// copy wav file
				err = utils.CopyFile(oldFname, newFname)
				if err != nil {
					log.Error(err)
					return
				}
				// copy info file
				err = utils.CopyFile(oldFname+".info", newFname+".info")
				if err != nil {
					log.Error(err)
					return
				}
			}
		}
	}

	// zip the folder
	cwd, _ := os.Getwd()
	os.Chdir(Storage)
	_, _, err = utils.Run("zip", "-r", zipFilename+".zip", zipFilename)
	if err != nil {
		log.Error(err)
	}
	os.Chdir(cwd)
	return
}
