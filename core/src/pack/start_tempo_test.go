package pack

import (
	"archive/zip"
	"encoding/json"
	"fmt"
	"io"
	"path/filepath"
	"testing"
)

func TestStartTempoDownloads(t *testing.T) {
	workspace := filepath.Join(t.TempDir(), "workspace")
	for _, settingsOnly := range []bool{false, true} {
		for _, value := range []string{"", "default", "30", "130", "300", "default"} {
			t.Run(fmt.Sprintf("settingsOnly=%t/tempo=%s", settingsOnly, value), func(t *testing.T) {
				payload := map[string]any{"settingsStartTempo": value, "banks": []any{}}
				body, _ := json.Marshal(payload)
				filename, err := Zip(workspace, body, settingsOnly)
				if err != nil {
					t.Fatal(err)
				}
				archive, err := zip.OpenReader(filename)
				if err != nil {
					t.Fatal(err)
				}
				defer archive.Close()
				file, err := archive.Open("workspace/settings/start_tempo")
				if err != nil {
					t.Fatal(err)
				}
				defer file.Close()
				content, err := io.ReadAll(file)
				want := value + "\n"
				if value == "" {
					want = "default\n"
				}
				if err != nil || string(content) != want {
					t.Fatalf("content=%q, want=%q, error=%v", content, want, err)
				}
			})
		}
	}
	for _, invalid := range []string{"0", "29", "301", "-130", "130.0", "0130", "130x", "garbage"} {
		body, _ := json.Marshal(map[string]any{"settingsStartTempo": invalid})
		if _, err := Zip(workspace, body, true); err == nil {
			t.Errorf("accepted invalid value %q", invalid)
		}
	}
}
