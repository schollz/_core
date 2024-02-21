package minicom

import (
	"strings"
	"time"

	"go.bug.st/serial/enumerator"

	log "github.com/schollz/logger"
	"go.bug.st/serial"
)

const COREID = "2E8A:1836"

var hasPort = false

func FoundPort() bool {
	return hasPort
}

// Run starts minicom and return a channel with string messages and channel for interrupting baudRateChange
func Run() (chan string, chan int, chan bool, error) {
	baudRateChange := make(chan int)
	dataChannel := make(chan []byte)
	chanPlugChange := make(chan bool)
	stopChannel := make(chan struct{})
	returnString := make(chan string)
	currentBaudRate := 115200 // Initial baud rate

	go serialPortReader(&currentBaudRate, baudRateChange, dataChannel, stopChannel, chanPlugChange)

	// Example usage
	go func() {
		for data := range dataChannel {
			dataString := strings.TrimSpace(string(data))
			if dataString != "" {
				returnString <- dataString
			}
		}
	}()

	return returnString, baudRateChange, chanPlugChange, nil

}

func serialPortReader(currentBaudRate *int, baudRateChange chan int, dataChannel chan []byte, stopChannel chan struct{}, chanPlugChange chan bool) {
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
				chanPlugChange <- hasPort
			}
			return
		}
		if len(ports) == 0 {
			log.Error("no ports found")
			if hasPort {
				hasPort = false
				chanPlugChange <- hasPort
			}
			return
		}
		portName := ""
		for _, port := range ports {
			id := strings.ToUpper(port.VID) + ":" + strings.ToUpper(port.PID)
			if id == ":" {
				continue
			}
			log.Tracef("found port %s with id %s", port.Name, id)
			if id == COREID {
				log.Tracef("found port %s", port.Name)
				portName = port.Name
				break
			}
		}
		if portName == "" {
			log.Trace("no port found")
			if hasPort {
				hasPort = false
				chanPlugChange <- hasPort
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
			if !hasPort {
				hasPort = true
				chanPlugChange <- hasPort
			}
		}
	}

	for {
		select {
		case <-stopChannel:
			if port != nil {
				port.Close()
			}
			close(dataChannel)
			return
		case newBaudRate := <-baudRateChange:
			log.Debugf("changing baud rate to %d", newBaudRate)
			*currentBaudRate = newBaudRate
			openPort(newBaudRate)
		default:
			if port == nil {
				openPort(*currentBaudRate)
			}
			if port == nil {
				time.Sleep(100 * time.Millisecond)
				log.Tracef("unable to open port")
				continue
			}

			buf := make([]byte, 128)
			port.SetReadTimeout(time.Millisecond * 100) // Set a short timeout for non-blocking read
			n, err := port.Read(buf)
			if err != nil {
				port = nil
				continue
			}
			if n > 0 {
				data := make([]byte, n)
				copy(data, buf[:n])
				dataChannel <- data
			}
		}
	}
}
