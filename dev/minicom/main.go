package main

import (
	"flag"
	"fmt"
	"net/http"
	"os"
	"os/signal"
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

	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	go func() {
		for sig := range c {
			fmt.Println(sig)
			fmt.Println("exiting")
			os.Exit(1)
		}
	}()

	go serialPortReader(&currentBaudRate, baudRateChange, dataChannel, stopChannel)

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

func serialPortReader(currentBaudRate *int, baudRateChange chan int, dataChannel chan []byte, stopChannel chan struct{}) {
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
		for portNum := 0; portNum < 3; portNum++ {
			portName := fmt.Sprintf("/dev/ttyACM%d", portNum)
			port, errconnect = serial.Open(portName, mode)
			if errconnect == nil {
				log.Debugf("opened port %s at %d", portName, baudRate)
				break
			}
		}
	}

	// create one second periodic timer
	timer := time.NewTicker(2 * time.Second)

	for {
		select {
		case <-timer.C:
			if port != nil {
				log.Tracef("sending v")
				port.Write([]byte("v"))
				port.SetReadTimeout(time.Millisecond * 100) // Set a short timeout for non-blocking read
				buf := make([]byte, 128)
				n, err := port.Read(buf)
				if err != nil {
					log.Debug("connection lost")
					port.Close()
					port = nil
				} else {
					data := make([]byte, n)
					copy(data, buf[:n])
					log.Tracef("read %s", string(data))
				}
			}
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
