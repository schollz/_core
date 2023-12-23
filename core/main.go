package main

import (
	"flag"

	"github.com/schollz/_core/core/src/server"
	log "github.com/schollz/logger"
)

var flagLogLevel string

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)
	log.SetLevel("trace")
	server.Serve()
}
