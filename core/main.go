package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"time"

	"github.com/schollz/_core/core/src/drumextract2"
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
var flagIsEctocore bool
var flagPort int
var EctocoreDefault = "no"

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
	flag.BoolVar(&flagUseFilesOnDisk, "usefiles", false, "use files on disk")
	flag.BoolVar(&flagDontOpen, "dontopen", false, "don't open browser")
	flag.BoolVar(&flagDontConnect, "dontconnect", false, "don't connect to core")
	flag.BoolVar(&flagIsEctocore, "ectocore", EctocoreDefault == "yes", "startup in ectocore mode")
	flag.IntVar(&flagPort, "port", 0, "server port (default: 8100 for ectocore, 8101 otherwise)")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)

	// Set port if specified via command line flag
	if flagPort > 0 {
		server.Port = flagPort
	} else if flagIsEctocore {
		server.Port = 8100
	}

	if !flagDontOpen {
		go func() {
			time.Sleep(2 * time.Second)
			utils.OpenBrowser(fmt.Sprintf("http://localhost:%d/tool", server.Port))
		}()
	}
	err := sox.Init()
	if err != nil {
		log.Error(err)
		time.Sleep(38 * time.Second)
		os.Exit(1)
	}

	// periodically clean the sox cache
	go func() {
		for {
			time.Sleep(10 * time.Minute)
			sox.Clean()
		}
	}()

	var chanString chan string
	var chanPrepareUpload chan bool
	var chanDeviceType chan string

	if !flagDontConnect {
		chanString, chanPrepareUpload, chanDeviceType, err = minicom.Run()
		if err != nil {
			log.Error(err)
		}
	}

	// detect if demucs program is available
	_, err = exec.LookPath("demucs")
	if err == nil {
		drumextract2.DownloadModel()
	}

	err = server.Serve(flagIsEctocore, flagUseFilesOnDisk, flagDontConnect, chanString, chanPrepareUpload, chanDeviceType)
	if err != nil {
		log.Error(err)
		time.Sleep(38 * time.Second)
		os.Exit(1)
	}
}
