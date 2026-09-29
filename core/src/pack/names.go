package pack

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"unicode/utf8"

	"github.com/schollz/_core/core/src/utils"
)

const maxNameMetadataBytes = 64 * 1024

type nameMetadata struct {
	Schema           int    `json:"schema"`
	Name             string `json:"name"`
	OriginalFilename string `json:"originalFilename"`
	AudioSHA256      string `json:"audioSha256"`
}

func writeNameMetadata(primary, sidecar, sourceFilename string) error {
	// Payloads may contain either platform's separators. Never persist directories.
	name := filepath.Base(strings.ReplaceAll(sourceFilename, "\\", "/"))
	if !utf8.ValidString(name) {
		return fmt.Errorf("filename is not UTF-8")
	}
	wav, err := os.Open(primary)
	if err != nil {
		return err
	}
	defer wav.Close()
	hash := sha256.New() // utils.HashFile uses a different algorithm.
	if _, err = io.Copy(hash, wav); err != nil {
		return err
	}
	data, err := json.MarshalIndent(nameMetadata{
		Schema: 1, Name: name, OriginalFilename: name,
		AudioSHA256: hex.EncodeToString(hash.Sum(nil)),
	}, "", "  ")
	if err != nil {
		return err
	}
	data = append(data, '\n')
	if len(data) > maxNameMetadataBytes {
		return fmt.Errorf("filename metadata exceeds 64 KiB")
	}
	return os.WriteFile(sidecar, data, 0666)
}

// Used by full packs after processing; settings-only exports never call this.
func copySample(storage, bankFolder string, slot int, filename string) error {
	stem := strings.TrimSuffix(filename, filepath.Ext(filename))
	for variant := 0; variant < 2; variant++ {
		source := filepath.Join(storage, filename, fmt.Sprintf("%s.%d.wav", stem, variant))
		target := filepath.Join(bankFolder, fmt.Sprintf("%d.%d.wav", slot, variant))
		if _, err := os.Stat(source); os.IsNotExist(err) && variant != 0 {
			break
		}
		if err := utils.CopyFile(source, target); err != nil {
			return err
		}
		if variant == 0 {
			sidecar := filepath.Join(bankFolder, fmt.Sprintf("%d.name.json", slot))
			if err := writeNameMetadata(target, sidecar, filename); err != nil {
				return fmt.Errorf("persist filename for bank sample %d: %w", slot, err)
			}
		}
		if err := utils.CopyFile(source+".info", target+".info"); err != nil {
			return err
		}
		for variation := 1; variation < 4; variation++ {
			source := filepath.Join(storage, filename, fmt.Sprintf("%s.%d.%d.wav", stem, variant, variation))
			target := filepath.Join(bankFolder, fmt.Sprintf("%d.%d.wav", slot, 2+variation*2+variant))
			if _, err := os.Stat(source); os.IsNotExist(err) {
				continue
			}
			if err := utils.CopyFile(source, target); err != nil {
				return err
			}
		}
	}
	return nil
}

// Preserve the pack's top-level folder without changing the server's cwd.
func writePackZip(folder, destination string) (err error) {
	file, err := os.Create(destination)
	if err != nil {
		return err
	}
	defer func() {
		if closeErr := file.Close(); err == nil {
			err = closeErr
		}
		if err != nil {
			os.Remove(destination)
		}
	}()
	w := zip.NewWriter(file)
	err = filepath.Walk(folder, func(path string, info os.FileInfo, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if info.IsDir() {
			return nil
		}
		source, err := os.Open(path)
		if err != nil {
			return err
		}
		defer source.Close()
		name, err := filepath.Rel(filepath.Dir(folder), path)
		if err != nil {
			return err
		}
		entry, err := w.Create(filepath.ToSlash(name))
		if err != nil {
			return err
		}
		_, err = io.Copy(entry, source)
		return err
	})
	if closeErr := w.Close(); err == nil {
		err = closeErr
	}
	return err
}
