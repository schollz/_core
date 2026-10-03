package pack

import (
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/schollz/_core/core/src/utils"
	"github.com/schollz/_core/core/src/zeptocore"
)

func fixtureFile(t *testing.T, path string, data []byte) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, data, 0644); err != nil {
		t.Fatal(err)
	}
}

// Same independent PCM fixture as the native adoption contract in Tests.cpp.
func websiteWAV() []byte {
	const frames = 88200
	b := make([]byte, 44+frames*2)
	copy(b, "RIFF")
	binary.LittleEndian.PutUint32(b[4:], uint32(len(b)-8))
	copy(b[8:], "WAVEfmt ")
	binary.LittleEndian.PutUint32(b[16:], 16)
	binary.LittleEndian.PutUint16(b[20:], 1)
	binary.LittleEndian.PutUint16(b[22:], 1)
	binary.LittleEndian.PutUint32(b[24:], 44100)
	binary.LittleEndian.PutUint32(b[28:], 88200)
	binary.LittleEndian.PutUint16(b[32:], 2)
	binary.LittleEndian.PutUint16(b[34:], 16)
	copy(b[36:], "data")
	binary.LittleEndian.PutUint32(b[40:], frames*2)
	for i := 0; i < frames; i++ {
		binary.LittleEndian.PutUint16(b[44+i*2:], uint16(i%12000))
	}
	return b
}

func readMetadata(t *testing.T, filename string) nameMetadata {
	t.Helper()
	b, err := os.ReadFile(filename)
	if err != nil {
		t.Fatal(err)
	}
	var metadata nameMetadata
	if err = json.Unmarshal(b, &metadata); err != nil {
		t.Fatal(err)
	}
	return metadata
}

func TestPack(t *testing.T) {
	storage := t.TempDir()
	fixtures := filepath.Join("..", "..", "..", "sample-manager", "Tests", "fixtures")
	want := readMetadata(t, filepath.Join(fixtures, "website-name.json"))
	name := want.OriginalFilename
	stem := strings.TrimSuffix(name, filepath.Ext(name))
	source := filepath.Join(storage, name, name)
	wav := websiteWAV()
	info, err := os.ReadFile(filepath.Join(fixtures, "website-1-44100.info"))
	if err != nil {
		t.Fatal(err)
	}
	fixtureFile(t, filepath.Join(storage, name, stem+".0.wav"), wav)
	fixtureFile(t, filepath.Join(storage, name, stem+".0.wav.info"), info)
	// Cached companions, including malformed metadata and alternative variants,
	// must be ignored without repair and omitted from fresh packs.
	obsolete := []string{stem + ".1.wav", stem + ".1.wav.info", stem + ".1.1.wav"}
	for _, name := range obsolete {
		fixtureFile(t, filepath.Join(storage, want.OriginalFilename, name), []byte("old companion"))
	}
	fixtureFile(t, filepath.Join(storage, name, stem+".0.1.wav"), wav)
	// Cache avoids running SoX or detection: this test exercises the real pack
	// writer with already completed numbered audio, as an export normally does.
	cached, err := json.Marshal(zeptocore.File{
		Filename: name, PathToFile: source, PathToAudio: source, Oversampling: 1,
	})
	if err != nil {
		t.Fatal(err)
	}
	fixtureFile(t, source+".json", cached)
	payload, err := json.Marshal(map[string]interface{}{
		"oversampling": "1x", "banks": []map[string]interface{}{{"files": []string{name, name}}},
	})
	if err != nil {
		t.Fatal(err)
	}
	archive, err := Zip(storage, payload, false)
	if err != nil {
		t.Fatal(err)
	}
	extracted := t.TempDir()
	if err := utils.Unzip(archive, extracted); err != nil {
		t.Fatal(err)
	}
	bank := filepath.Join(extracted, filepath.Base(storage), "bank1")
	for _, slot := range []string{"0", "1"} {
		got := readMetadata(t, filepath.Join(bank, slot+".name.json"))
		if got != want {
			t.Fatalf("website pack disagrees with native adoption fixture: got %+v, want %+v", got, want)
		}
		b, err := os.ReadFile(filepath.Join(bank, slot+".0.wav"))
		if err != nil {
			t.Fatal(err)
		}
		hash := sha256.Sum256(b)
		if hex.EncodeToString(hash[:]) != got.AudioSHA256 {
			t.Fatal("sidecar must hash all extracted primary WAV bytes")
		}
	}
	for _, slot := range []string{"0", "1"} {
		for _, variant := range []string{"1", "3", "5", "7", "9"} {
			if _, err := os.Stat(filepath.Join(bank, slot+"."+variant+".wav")); !os.IsNotExist(err) {
				t.Fatal("companion exported", slot, variant)
			}
		}
		if _, err := os.Stat(filepath.Join(bank, slot+".4.wav")); err != nil {
			t.Fatal("normal-speed alternative lost", err)
		}
	}
	for _, obsoleteName := range obsolete {
		if got, err := os.ReadFile(filepath.Join(storage, name, obsoleteName)); err != nil || string(got) != "old companion" {
			t.Fatal("cached companion modified", err)
		}
	}
	settingsArchive, err := Zip(storage, payload, true)
	if err != nil {
		t.Fatal(err)
	}
	settings := t.TempDir()
	if err := utils.Unzip(settingsArchive, settings); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(settings, filepath.Base(storage), "bank1")); !os.IsNotExist(err) {
		t.Fatal("settings-only pack contains sample files")
	}
}

func TestNameMetadataFailures(t *testing.T) {
	root := t.TempDir()
	primary := filepath.Join(root, "0.0.wav")
	fixtureFile(t, primary, websiteWAV())
	sidecar := filepath.Join(root, "0.name.json")
	for _, source := range []string{"/private/folder/Amen — 鼓.wav", `C:\private\folder\Amen — 鼓.wav`} {
		if err := writeNameMetadata(primary, sidecar, source); err != nil {
			t.Fatal(err)
		}
		got := readMetadata(t, sidecar)
		if got.Name != "Amen — 鼓.wav" || got.OriginalFilename != got.Name {
			t.Fatal("only the imported basename should be persisted")
		}
	}
	if err := writeNameMetadata(primary, sidecar, strings.Repeat("x", maxNameMetadataBytes)); err == nil {
		t.Fatal("oversized metadata must fail")
	}
	if err := writeNameMetadata(primary, root, "input.wav"); err == nil {
		t.Fatal("unwritable sidecar must fail")
	}
	storage := filepath.Join(root, "source")
	fixtureFile(t, filepath.Join(storage, "input.wav", "input.0.wav"), websiteWAV())
	bank := filepath.Join(root, "bank1")
	if err := os.MkdirAll(filepath.Join(bank, "0.name.json"), 0755); err != nil {
		t.Fatal(err)
	}
	if err := copySample(storage, bank, 0, "input.wav"); err == nil || !strings.Contains(err.Error(), "persist filename") {
		t.Fatalf("sample export must propagate sidecar write failure: %v", err)
	}
	if err := copySample(storage, bank, 1, "missing.wav"); err == nil {
		t.Fatal("full packs require primary audio")
	}
}
