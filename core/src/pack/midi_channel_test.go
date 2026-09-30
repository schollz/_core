package pack

import (
	"archive/zip"
	"encoding/json"
	"fmt"
	"io"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

func TestMIDIChannelDownloads(t *testing.T) {
	workspace := filepath.Join(t.TempDir(), "workspace")
	for _, settingsOnly := range []bool{false, true} {
		// Reuse the workspace to exercise changing the mode back to the default.
		for _, mapping := range []string{"", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "0", "17", "01", "invalid"} {
			t.Run(fmt.Sprintf("settingsOnly=%t/mapping=%s", settingsOnly, mapping), func(t *testing.T) {
				payload := map[string]any{"settingsSampleCV": "bipolar", "banks": []any{}}
				if mapping != "" {
					payload["settingsMIDIChannel"] = mapping
				}
				body, err := json.Marshal(payload)
				if err != nil {
					t.Fatal(err)
				}
				filename, err := Zip(workspace, body, settingsOnly)
				if err != nil {
					t.Fatal(err)
				}
				archive, err := zip.OpenReader(filename)
				if err != nil {
					t.Fatal(err)
				}
				defer archive.Close()
				want := "1\n"
				if n, e := strconv.Atoi(mapping); e == nil && n >= 1 && n <= 16 && strconv.Itoa(n) == mapping {
					want = mapping + "\n"
				}
				found := 0
				for _, file := range archive.File {
					if !strings.Contains(file.Name, "midi_channel") {
						continue
					}
					found++
					if file.Name != "workspace/settings/midi_channel" {
						t.Fatalf("unexpected setting path: %s", file.Name)
					}
					r, err := file.Open()
					if err != nil {
						t.Fatal(err)
					}
					content, err := io.ReadAll(r)
					r.Close()
					if err != nil || string(content) != want {
						t.Fatalf("mapping = %q, want %q; error: %v", content, want, err)
					}
				}
				if found != 1 {
					t.Fatalf("got %d mapping files, want exactly one", found)
				}
			})
		}
	}
}
