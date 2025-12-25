//go:build !linux
// +build !linux

package minicom

const ZEPTOCOREID = "2E8A:1836"
const EZEPTOCOREID = "2E8A:1837"
const BOARDCOREID = "2E8A:1838"

var hasPort = false

func FoundPort() bool {
	return false
}

// Run starts minicom and return a channel with string messages and channel for interrupting baudRateChange
func Run() (chan string, chan bool, chan string, error) {
	return nil, nil, nil, nil
}
