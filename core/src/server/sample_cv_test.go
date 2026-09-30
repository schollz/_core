package server

import (
	"strings"
	"testing"
)

func TestSampleCVMappingDeviceVisibility(t *testing.T) {
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
			present := strings.Contains(html, `id="settingsSampleCVMapping"`)
			if present == device.zeptocore {
				t.Fatalf("Sample CV mapping visible = %t", present)
			}
			if present {
				for _, option := range []string{`<option value="bank">Bank divisions</option>`, `<option value="1voct">1 V/oct</option>`} {
					if !strings.Contains(html, option) {
						t.Fatalf("missing option: %s", option)
					}
				}
			}
		})
	}
}
