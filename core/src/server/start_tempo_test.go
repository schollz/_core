package server

import (
	"strings"
	"testing"
)

func TestStartTempoVisibleOnAllDevices(t *testing.T) {
	for _, device := range []struct {
		host  string
		zepto bool
	}{
		{"zeptocore.com", true}, {"ezeptocore.com", false}, {"ectocore.rocks", false},
	} {
		t.Run(device.host, func(t *testing.T) {
			html := renderStaticIndexForHost(t, device.zepto, device.host)
			for _, expected := range []string{`id="settingsStartTempo"`, `id="startTempoBpm"`, `min="30" max="300" step="1"`, `Default (no override)`, `Fixed BPM`} {
				if !strings.Contains(html, expected) {
					t.Errorf("missing %s", expected)
				}
			}
		})
	}
}
