//go:build linux
// +build linux

package minicom

import (
	"bytes"
	"strings"
	"time"

	"go.bug.st/serial/enumerator"

	log "github.com/schollz/logger"
	"go.bug.st/serial"
)

const ZEPTOCOREID = "2E8A:1836"
const EZEPTOCOREID = "2E8A:1837"
const BOARDCOREID = "2E8A:1838"

var hasPort = false

func FoundPort() bool {
	return hasPort
}

// Run starts minicom and return a channel with string messages and channel for interrupting baudRateChange
func Run() (chan string, chan bool, chan string, error) {
	prepareUpload := make(chan bool)
	dataChannel := make(chan []byte)
	chanDeviceType := make(chan string)
	stopChannel := make(chan struct{})
	returnString := make(chan string)

	// Example usage
	go func() {
		for data := range dataChannel {
			dataString := strings.TrimSpace(string(data))
			if dataString != "" {
				returnString <- dataString
			}
		}
	}()

	go func() {
		time.Sleep(2 * time.Second)
		serialPortReader(prepareUpload, dataChannel, stopChannel, chanDeviceType)
	}()

	return returnString, prepareUpload, chanDeviceType, nil

}

func serialPortReader(prepareUpload chan bool, dataChannel chan []byte, stopChannel chan struct{}, chanDeviceType chan string) {
	var port serial.Port

	openPort := func(baudRate int) {
		if port != nil {
			port.Close()
			port = nil
		}

		ports, err := enumerator.GetDetailedPortsList()
		if err != nil {
			log.Error(err)
			if hasPort {
				hasPort = false
				chanDeviceType <- ""
			}
			return
		}
		if len(ports) == 0 {
			log.Trace("no ports found")
			if hasPort {
				hasPort = false
				chanDeviceType <- ""
			}
			return
		}
		portName := ""
		deviceType := ""
		for _, port := range ports {
			id := strings.ToUpper(port.VID) + ":" + strings.ToUpper(port.PID)
			if id == ":" {
				continue
			}
			log.Tracef("found port %s with id %s", port.Name, id)
			if id == ZEPTOCOREID || id == EZEPTOCOREID || id == BOARDCOREID {
				if id == ZEPTOCOREID {
					deviceType = "zeptocore"
				} else if id == EZEPTOCOREID {
					deviceType = "ezeptocore"
				} else if id == BOARDCOREID {
					deviceType = "zeptoboard"
				}
				log.Tracef("found port %s", port.Name)
				portName = port.Name
				break
			}
		}
		if portName == "" {
			// log.Trace("no port found")
			if hasPort {
				hasPort = false
				chanDeviceType <- ""
			}
			return
		}

		mode := &serial.Mode{
			BaudRate: baudRate,
		}
		var errconnect error
		port, errconnect = serial.Open(portName, mode)
		if errconnect == nil {
			log.Debugf("opened port %s at %d", portName, baudRate)
			// send a message to the port
			port.Write([]byte("v"))
			if !hasPort {
				hasPort = true
				chanDeviceType <- deviceType
			}
		}
	}

	// Initialize a buffer to accumulate data outside the loop if not already done.
	var accumulatedData []byte

	for {
		select {
		case <-stopChannel:
			if port != nil {
				port.Close()
			}
			close(dataChannel)
			return
		case <-prepareUpload:
			log.Debug("preparing upload")
			log.Debugf("opening port at 1200")
			openPort(1200)
			time.Sleep(1 * time.Second)
			if port != nil {
				port.Close()
				port = nil
			}
			time.Sleep(4 * time.Second)
			openPort(115200)
		default:
			if port == nil {
				openPort(115200)
			}
			if port == nil {
				time.Sleep(250 * time.Millisecond)
				// log.Tracef("unable to open port")
				continue
			}

			buf := make([]byte, 1024*4)
			port.SetReadTimeout(time.Millisecond * 100) // Set a short timeout for non-blocking read
			n, err := port.Read(buf)
			if err != nil {
				port = nil
				continue
			}
			if n > 0 {
				// Append read data to the accumulated buffer.
				accumulatedData = append(accumulatedData, buf[:n]...)

				// Check if accumulatedData contains a newline.
				if i := bytes.IndexByte(accumulatedData, '\n'); i >= 0 {
					// Split at the newline; send the first part to dataChannel.
					dataToSend := accumulatedData[:i]
					dataChannel <- dataToSend

					// Keep the remaining data for the next read cycle.
					accumulatedData = accumulatedData[i+1:]
				}
			}
		}
	}
}
