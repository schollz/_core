package main

import (
	"fmt"
	"os"
	"os/signal"
	"strings"
	"time"

	log "github.com/schollz/logger" // autoregisters driver
	"gitlab.com/gomidi/midi/v2"

	//_ "gitlab.com/gomidi/midi/v2/drivers/portmididrv" // autoregisters driver
	"gitlab.com/gomidi/midi/v2/drivers"
	_ "gitlab.com/gomidi/midi/v2/drivers/rtmididrv"
)

func parseSysExToString(sysex []byte) (string, error) {
	if len(sysex) < 3 || sysex[0] != 0xF0 || sysex[len(sysex)-1] != 0xF7 {
		return "", fmt.Errorf("invalid SysEx message")
	}
	messageBytes := sysex[1 : len(sysex)-1]
	message := string(messageBytes)
	return message, nil
}

var isConnected = false

func isAvailable() bool {
	return strings.Contains(midi.GetInPorts().String(), "zeptocore")
}

func doConnection() (stop func(), err error) {
	var midiInput drivers.In
	ins := midi.GetInPorts()
	if len(ins) == 0 {
		log.Error("no input devices")
		return
	}
	for _, in := range ins {
		log.Debugf("found input: '%s'", in.String())
		if strings.HasPrefix(in.String(), "zeptocore") {
			midiInput = in
			break
		}
	}
	if midiInput == nil {
		log.Error("no input devices")
		return
	}

	// listen to midi
	stop, err = midi.ListenTo(midiInput, func(msg midi.Message, timestamps int32) {
		var bt []byte
		var ch, key, vel uint8
		switch {
		case msg.GetSysEx(&bt):
			fmt.Printf("%s", bt)
		case msg.GetNoteStart(&ch, &key, &vel):
			log.Debugf("note_on=%s, ch=%v, vel=%v\n", midi.Note(key), ch, vel)
		case msg.GetNoteEnd(&ch, &key):
			log.Debugf("note_off=%s, ch=%v\n", midi.Note(key), ch)
		default:
			// ignore
		}
	}, midi.UseSysEx(), midi.SysExBufferSize(4096))

	if err != nil {
		log.Error(err)
		return
	}
	isConnected = true
	return
}

func main() {
	var err error
	done := make(chan struct{})
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	go func() {
		for sig := range c {
			done <- struct{}{}
			time.Sleep(100 * time.Millisecond)
			log.Infof("captured %v, exiting..", sig)
			os.Exit(0)
		}
	}()

	var stopFunc func()
	go func() {
		// check if midi is connected
		for {
			if isConnected {
				if !isAvailable() {
					isConnected = false
					log.Debug("disconnected")
					midi.CloseDriver()
				}
			} else {
				if isAvailable() {
					stopFunc, err = doConnection()
					if err != nil {
						log.Error(err)
					} else {
						log.Debug("connected")
					}
				}
			}
			time.Sleep(100 * time.Millisecond)
		}
	}()

	for {
		select {
		case <-done:
			stopFunc()
		}
	}

}
