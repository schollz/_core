package main

import (
	"fmt"
	"math/rand"
	"time"

	"github.com/rakyll/portmidi"
	log "github.com/schollz/logger"
)

func parseSysExToString(sysex []byte) (string, error) {
	if len(sysex) < 3 || sysex[0] != 0xF0 || sysex[len(sysex)-1] != 0xF7 {
		return "", fmt.Errorf("invalid SysEx message")
	}
	messageBytes := sysex[1 : len(sysex)-1]
	message := string(messageBytes)
	return message, nil
}

func main() {
	devices := portmidi.CountDevices()
	outputNum := 0
	inputNum := 0
	for deviceID := 0; deviceID < devices; deviceID++ {
		fmt.Printf("%+v\n", portmidi.Info(portmidi.DeviceID(deviceID)))
		if portmidi.Info(portmidi.DeviceID(deviceID)).IsOutputAvailable {
			outputNum = deviceID
		}
		if portmidi.Info(portmidi.DeviceID(deviceID)).IsInputAvailable {
			inputNum = deviceID
		}
	}
	fmt.Printf("Output Device: %v\n", outputNum)
	fmt.Printf("Input Device: %v\n", inputNum)

	go func() {
		in, err := portmidi.NewInputStream(portmidi.DeviceID(inputNum), 1024)
		if err != nil {
			log.Error(err)
		}
		defer in.Close()

		for {
			events, err := in.Read(1024)
			if err != nil {
				log.Error(err)
			}
			for _, event := range events {
				log.Debugf("event: %+v", event)
				if len(event.SysEx) > 0 {
					s, err := parseSysExToString(event.SysEx)
					if err != nil {
						log.Error(err)
					} else {
						log.Debugf("SysEx: %s", s)
					}
				}
			}
			time.Sleep(100 * time.Microsecond)
		}
	}()
	out, err := portmidi.NewOutputStream(portmidi.DeviceID(outputNum), 1024, 0)
	if err != nil {
		log.Error(err)
	}
	for i := 0; i < 3; i++ {
		// generate random note between 0 and 127
		note := rand.Intn(126)
		velocity := rand.Intn(126)
		log.Debugf("writing note %d with velocity %d", note, velocity)
		out.WriteShort(0x90, int64(note), int64(velocity))
		time.Sleep(1 * time.Second)
	}
	out.WriteShort(0x80, 0, 0)
	time.Sleep(3 * time.Second)
}
