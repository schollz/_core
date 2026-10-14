package pack

import (
	"archive/zip"
	"encoding/json"
	"fmt"
	"io"
	"path/filepath"
	"strings"
	"testing"
)

func TestStartupPlaybackDownloads(t *testing.T) {
	workspace := filepath.Join(t.TempDir(), "workspace")
	for _, settingsOnly := range []bool{false, true} {
		// Reuse the same workspace to verify wait can be replaced by play.
		for _, value := range []string{"", "wait", "play", "invalid"} {
			t.Run(fmt.Sprintf("settingsOnly=%t/mode=%s", settingsOnly, value), func(t *testing.T) {
				body, _ := json.Marshal(map[string]any{"settingsStartupPlayback": value, "banks": []any{}})
				filename, err := Zip(workspace, body, settingsOnly)
				if err != nil {
					t.Fatal(err)
				}
				archive, err := zip.OpenReader(filename)
				if err != nil {
					t.Fatal(err)
				}
				defer archive.Close()
				want := "play\n"
				if value == "wait" {
					want = "wait\n"
				}
				found := 0
				for _, file := range archive.File {
					if !strings.Contains(file.Name, "startup_playback") {
						continue
					}
					found++
					if file.Name != "workspace/settings/startup_playback" {
						t.Fatalf("unexpected path: %s", file.Name)
					}
					r, err := file.Open()
					if err != nil {
						t.Fatal(err)
					}
					content, err := io.ReadAll(r)
					r.Close()
					if err != nil || string(content) != want {
						t.Fatalf("mode = %q, want %q; error: %v", content, want, err)
					}
				}
				if found != 1 {
					t.Fatalf("got %d startup files, want one", found)
				}
			})
		}
	}
}
