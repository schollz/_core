package detectdevice

import (
	"fmt"
	"log"
	"testing"

	"go.bug.st/serial/enumerator"
)

func TestDetectDevice(t *testing.T) {
	ports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Fatal(err)
	}
	if len(ports) == 0 {
		fmt.Println("No serial ports found!")
		return
	}
	for _, port := range ports {
		fmt.Printf("Found port: %s\n", port.Name)
		fmt.Printf("   IsUSB      %v\n", port.IsUSB)
		if port.IsUSB {
			fmt.Printf("   USB ID     %s:%s\n", port.VID, port.PID)
			fmt.Printf("   USB serial %s\n", port.SerialNumber)
		}
	}
}
