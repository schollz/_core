package latestrelease

import (
	"fmt"
	"testing"
)

func TestGet(t *testing.T) {
	filename, err := DownloadZeptocore()
	if err != nil {
		t.Error(err)
	}
	fmt.Println(filename)
}
