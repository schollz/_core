package server

import (
	"fmt"
	"strings"
	"testing"
)

func TestMIDIChannelDeviceVisibility(t *testing.T) {
	for _, device := range []struct {
		host      string
		zeptocore bool
	}{
		{"ezeptocore.com", false},
		{"ectocore.rocks", false},
		{"zeptocore.com", true},
	} {
		t.Run(device.host, func(t *testing.T) {
			html := renderStaticIndexForHost(t, device.zeptocore, device.host)
			present := strings.Contains(html, `id="settingsMidiChannel"`)
			if present != device.zeptocore {
				t.Fatalf("MIDI channel visible = %t", present)
			}
			if present {
				for n := 1; n <= 16; n++ {
					option := fmt.Sprintf(`<option value="%d">%d</option>`, n, n)
					if !strings.Contains(html, option) {
						t.Fatalf("missing option: %s", option)
					}
				}
			}
		})
	}
}
