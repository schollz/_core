package latestrelease

import (
	"fmt"
	"testing"
)

func TestGet(t *testing.T) {
	filename, err := Tag()
	if err != nil {
		t.Error(err)
	}
	fmt.Println(filename)
}
