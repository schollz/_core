package main

import (
	"bufio"
	"fmt"
	"io"
	"net/http"
	"os/exec"
	"strings"
	"time"

	"github.com/jacobsa/go-serial/serial"
	log "github.com/schollz/logger"
)

var lostConnection = false
var lastMessageTime time.Time
var startConnectionTime time.Time
var notNotified = true

func main() {
	startConnectionTime = time.Now()
	go func() {
		for {
			time.Sleep(1 * time.Second)
			if (time.Since(startConnectionTime)) > 10*time.Minute && notNotified && time.Since(lastMessageTime) < 5*time.Second {
				notNotified = false
				var currentHash string
				cmd := exec.Command("git", "rev-parse", "--short", "HEAD")
				if out, err := cmd.Output(); err == nil {
					currentHash = string(out)
				}

				http.Post("https://ntfy.sh/qrv3w", "text/plain",
					strings.NewReader(currentHash+" ok"))

			}
			if time.Since(lastMessageTime) > 5*time.Second && !lostConnection {
				log.Info("lost connection")
				lostConnection = true
				// get current hash using
				// git rev-parse --short HEAD
				var currentHash string
				cmd := exec.Command("git", "rev-parse", "--short", "HEAD")
				if out, err := cmd.Output(); err == nil {
					currentHash = string(out)
				}

				http.Post("https://ntfy.sh/qrv3w", "text/plain",
					strings.NewReader(currentHash+" bad"))
			}
		}

	}()
	for {
		listenToMessages()
		time.Sleep(1 * time.Second)
	}
}

func listenToMessages() {
	// Serial port options
	options := serial.OpenOptions{
		PortName:        "/dev/ttyACM0", // Replace with your port name
		BaudRate:        115200,
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: 4,
	}

	// Open the port
	port, err := serial.Open(options)
	if err != nil {
		log.Error(err)
		return
	}
	defer port.Close()

	// Create a buffered reader
	reader := bufio.NewReader(port)
	startConnectionTime = time.Now()
	notNotified = true
	// Continuously read and print messages from the serial port
	for {
		message, err := reader.ReadString('\n')
		if err != nil {
			if err != io.EOF {
				log.Error(err)
				return
			}
			break
		}
		fmt.Print(message)
		lastMessageTime = time.Now()
		lostConnection = false
	}
}
