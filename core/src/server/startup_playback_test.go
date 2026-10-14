package server

import (
	"strings"
	"testing"
)

func TestStartupPlaybackProductVisibility(t *testing.T) {
	for _, device := range []struct {
		host  string
		zepto bool
	}{
		{"zeptocore.com", true}, {"ezeptocore.com", false}, {"ectocore.rocks", false},
	} {
		t.Run(device.host, func(t *testing.T) {
			html := renderStaticIndexForHost(t, device.zepto, device.host)
			if strings.Contains(html, `id="settingsStartupPlayback"`) == device.zepto {
				t.Fatal("incorrect startup setting visibility")
			}
			if !device.zepto {
				for _, text := range []string{`value="play"`, `value="wait"`, "Play immediately (default)", "Wait for clock or button"} {
					if !strings.Contains(html, text) {
						t.Errorf("missing %s", text)
					}
				}
			}
		})
	}
}
