package main

import (
	"flag"

	log "github.com/schollz/logger"
	"github.com/schollz/zeptoconverter/cmd/zeptoserver/include/server"
)

var flagLogLevel string

func init() {
	flag.StringVar(&flagLogLevel, "log", "debug", "log level (trace, debug, info)")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogLevel)
	server.Serve()
}
