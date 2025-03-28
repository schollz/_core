package detectdisks

import (
	"fmt"
	"testing"
)

func TestDetect(t *testing.T) {
	uf2disk, err := GetUF2Drive()
	if err != nil {
		t.Error(err)
	}
	fmt.Println(uf2disk)
}
