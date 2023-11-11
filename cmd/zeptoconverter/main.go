package main

import (
	"flag"
	"os"

	log "github.com/schollz/logger"
	"github.com/schollz/zeptocore/cmd/zeptoconverter/include/convert"
)

func main() {
	var flagOversampling int
	var flagFolderIn, flagFolderOut string
	var flagStereo bool
	flag.StringVar(&flagFolderIn, "in", "", "folder for input")
	flag.StringVar(&flagFolderOut, "out", "", "folder for output")
	flag.BoolVar(&flagStereo, "stereo", false, "stereo")
	flag.IntVar(&flagOversampling, "oversampling", 1, "number of times (1, 2 or 4)")
	flag.Parse()
	log.SetLevel("debug")
	err := convert.Convert(convert.Converter{
		FolderIn:     flagFolderIn,
		FolderOut:    flagFolderOut,
		Stereo:       flagStereo,
		Oversampling: flagOversampling,
	})
	if err != nil {
		log.Error(err)
		os.Exit(1)
	}
}
