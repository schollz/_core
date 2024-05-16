package main

import (
	"flag"
	"os"
	"time"

	"github.com/schollz/_core/core/src/minicom"
	"github.com/schollz/_core/core/src/server"
	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
)

var flagLogLevel string
var flagDontOpen bool
var flagUseFilesOnDisk bool
var flagDontConnect bool

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
	flag.BoolVar(&flagUseFilesOnDisk, "usefiles", false, "use files on disk")
	flag.BoolVar(&flagDontOpen, "dontopen", false, "don't open browser")
	flag.BoolVar(&flagDontConnect, "dontconnect", false, "don't connect to core")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)
	if !flagDontOpen {
		utils.OpenBrowser("http://localhost:8101/tool")
	}
	err := sox.Init()
	if err != nil {
		log.Error(err)
		time.Sleep(38 * time.Second)
		os.Exit(1)
	}

	var chanString chan string
	var chanPrepareUpload chan bool
	var chanDeviceType chan string

	if !flagDontConnect {
		chanString, chanPrepareUpload, chanDeviceType, err = minicom.Run()
		if err != nil {
			log.Error(err)
		}
	}

	err = server.Serve(flagUseFilesOnDisk, flagDontConnect, chanString, chanPrepareUpload, chanDeviceType)
	if err != nil {
		log.Error(err)
		time.Sleep(38 * time.Second)
		os.Exit(1)
	}
}
