package utils

import (
	"archive/zip"
	"os"
	"path/filepath"
	"runtime"
	"testing"
)

func TestUnzipCreatesTraversableParents(t *testing.T) {
	for _, mode := range []os.FileMode{0644, 0444} {
		t.Run(mode.String(), func(t *testing.T) {
			root := t.TempDir()
			archive := filepath.Join(root, "samples.zip")
			file, err := os.Create(archive)
			if err != nil {
				t.Fatal(err)
			}
			writer := zip.NewWriter(file)
			// A pack can contain only files, with no explicit parent directories.
			for _, name := range []string{"card/settings/start_tempo", "card/bank1/0.name.json"} {
				header := &zip.FileHeader{Name: name}
				header.SetMode(mode)
				entry, err := writer.CreateHeader(header)
				if err != nil {
					t.Fatal(err)
				}
				if _, err := entry.Write([]byte(name)); err != nil {
					t.Fatal(err)
				}
			}
			if err := writer.Close(); err != nil {
				t.Fatal(err)
			}
			if err := file.Close(); err != nil {
				t.Fatal(err)
			}
			dest := filepath.Join(root, "extracted")
			if err := Unzip(archive, dest); err != nil {
				t.Fatal(err)
			}
			for _, name := range []string{"card/settings/start_tempo", "card/bank1/0.name.json"} {
				path := filepath.Join(dest, name)
				data, err := os.ReadFile(path)
				if err != nil || string(data) != name {
					t.Fatalf("extracted %s: %q, %v", name, data, err)
				}
				if runtime.GOOS != "windows" {
					info, err := os.Stat(path)
					if err != nil || info.Mode().Perm() != mode {
						t.Fatalf("file permissions should remain %v: %v, %v", mode, info, err)
					}
					parent, err := os.Stat(filepath.Dir(path))
					if err != nil || parent.Mode().Perm()&0100 == 0 {
						t.Fatalf("parent must be traversable: %v, %v", parent, err)
					}
				}
			}
		})
	}
}

func TestUnzipReturnsDirectoryErrors(t *testing.T) {
	root := t.TempDir()
	archive := filepath.Join(root, "directory.zip")
	file, err := os.Create(archive)
	if err != nil {
		t.Fatal(err)
	}
	writer := zip.NewWriter(file)
	if _, err := writer.Create("blocked/"); err != nil {
		t.Fatal(err)
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}
	if err := file.Close(); err != nil {
		t.Fatal(err)
	}
	dest := t.TempDir()
	if err := os.WriteFile(filepath.Join(dest, "blocked"), []byte("existing file"), 0644); err != nil {
		t.Fatal(err)
	}
	if err := Unzip(archive, dest); err == nil {
		t.Fatal("a directory entry blocked by an existing file must fail extraction")
	}
}
