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
var flagIsEzeptocore bool
var flagPort int

// DefaultProduct can be set at build time for branded offline packages.
var DefaultProduct = "zeptocore"

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
	flag.BoolVar(&flagUseFilesOnDisk, "usefiles", false, "use files on disk")
	flag.BoolVar(&flagDontOpen, "dontopen", false, "don't open browser")
	flag.BoolVar(&flagDontConnect, "dontconnect", false, "don't connect to core")
	flag.BoolVar(&flagIsEctocore, "ectocore", false, "use the blue ectocore website with runes (overrides the packaged default)")
	flag.BoolVar(&flagIsEzeptocore, "ezeptocore", false, "use the gray ezeptocore website with Roman numerals (overrides the packaged default)")
	flag.IntVar(&flagPort, "port", 0, "server port (default: 8100 for ectocore/ezeptocore, 8101 for zeptocore)")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage of %s (default website: %s):\n", os.Args[0], DefaultProduct)
		flag.PrintDefaults()
	}
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)

	product, err := selectedProduct()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}

	// Set port if specified via command line flag
	if flagPort > 0 {
		server.Port = flagPort
	} else if product != server.ProductZeptocore {
		server.Port = 8100
	}

	if !flagDontOpen {
		go func() {
			time.Sleep(2 * time.Second)
			utils.OpenBrowser(fmt.Sprintf("http://localhost:%d/tool", server.Port))
		}()
	}
	err = sox.Init()
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

	err = server.Serve(product, flagUseFilesOnDisk, flagDontConnect, chanString, chanPrepareUpload, chanDeviceType)
	if err != nil {
		log.Error(err)
		time.Sleep(38 * time.Second)
		os.Exit(1)
	}
}

func selectedProduct() (server.Product, error) {
	if flagIsEctocore && flagIsEzeptocore {
		return "", fmt.Errorf("-ectocore and -ezeptocore cannot both be enabled; choose one website mode")
	}

	product := server.Product(DefaultProduct)
	// Any explicit product flag replaces the package default. False flags alone
	// select Zeptocore, including in a branded offline package.
	flag.Visit(func(f *flag.Flag) {
		if f.Name == "ectocore" || f.Name == "ezeptocore" {
			product = server.ProductZeptocore
		}
	})
	if flagIsEctocore {
		product = server.ProductEctocore
	} else if flagIsEzeptocore {
		product = server.ProductEzeptocore
	}

	switch product {
	case server.ProductZeptocore, server.ProductEzeptocore, server.ProductEctocore:
		return product, nil
	default:
		return "", fmt.Errorf("unknown default website mode %q", product)
	}
}
