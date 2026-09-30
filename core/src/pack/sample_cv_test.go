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

func TestSampleCVMappingDownloads(t *testing.T) {
	workspace := filepath.Join(t.TempDir(), "workspace")
	for _, settingsOnly := range []bool{false, true} {
		// Reuse the workspace to exercise changing the mode back to the default.
		for _, mapping := range []string{"", "1voct", "bank", "invalid"} {
			t.Run(fmt.Sprintf("settingsOnly=%t/mapping=%s", settingsOnly, mapping), func(t *testing.T) {
				payload := map[string]any{"settingsSampleCV": "bipolar", "banks": []any{}}
				if mapping != "" {
					payload["settingsSampleCVMapping"] = mapping
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
				want := "bank\n"
				if mapping == "1voct" {
					want = "1voct\n"
				}
				found := 0
				for _, file := range archive.File {
					if !strings.Contains(file.Name, "sample_cv_mapping") {
						continue
					}
					found++
					if file.Name != "workspace/settings/sample_cv_mapping" {
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
