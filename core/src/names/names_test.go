package names

import (
	"fmt"
	"testing"
)

func TestNames(t *testing.T) {
	name := Random()
	fmt.Println(name)
	if name == "" {
		t.Errorf("name should not be empty")
	}
}
