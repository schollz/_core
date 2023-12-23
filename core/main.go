package main

import (
	"flag"

	"github.com/schollz/_core/core/src/server"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
)

var flagLogLevel string
var flagDontOpen bool

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
	flag.BoolVar(&flagDontOpen, "dontopen", false, "don't open browser")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)
	if !flagDontOpen {
		utils.OpenBrowser("http://localhost:8101")
	}
	server.Serve()
}
