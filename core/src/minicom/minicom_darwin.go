package minicom

func FoundPort() bool {
	return false
}

// Run starts minicom and return a channel with string messages and channel for interrupting baudRateChange
func Run() (chan string, chan bool, chan string, error) {
	prepareUpload := make(chan bool)
	chanDeviceType := make(chan string)
	returnString := make(chan string)
	return returnString, prepareUpload, chanDeviceType, nil
}
