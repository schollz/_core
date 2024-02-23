package main

import (
	"flag"
	"fmt"
	"net/http"
	"strings"
	"time"

	log "github.com/schollz/logger"
	"go.bug.st/serial"
)

var flagLogger string

func init() {
	flag.StringVar(&flagLogger, "logger", "debug", "logger level")
}

func main() {
	flag.Parse()
	log.SetLevel(flagLogger)
	log.Info("starting minicom")

	baudRateChange := make(chan int)
	dataChannel := make(chan []byte)
	stopChannel := make(chan struct{})
	currentBaudRate := 115200 // Initial baud rate

	go serialPortReader("/dev/ttyACM0", &currentBaudRate, baudRateChange, dataChannel, stopChannel)

	// Example usage
	go func() {
		for data := range dataChannel {
			dataString := strings.TrimSpace(string(data))
			if dataString != "" {
				fmt.Println(dataString)
			}
		}
	}()

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("done"))
		go func() {
			baudRateChange <- 1200 // New baud rate
			time.Sleep(5 * time.Second)
			baudRateChange <- 115200 // New baud rate

		}()
	})

	log.Debugf("running on port 7083")
	http.ListenAndServe(":7083", nil)
}

func serialPortReader(portName string, currentBaudRate *int, baudRateChange chan int, dataChannel chan []byte, stopChannel chan struct{}) {
	var port serial.Port

	openPort := func(baudRate int) {
		if port != nil {
			port.Close()
			port = nil
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
				log.Tracef("unable to open port %s", portName)
				continue
			}

			buf := make([]byte, 128)
			port.SetReadTimeout(time.Millisecond * 100) // Set a short timeout for non-blocking read
			n, err := port.Read(buf)
			if err != nil {
				continue // Handle as needed; for now, just retry
			}
			if n > 0 {
				data := make([]byte, n)
				copy(data, buf[:n])
				dataChannel <- data
			}
		}
	}
}
