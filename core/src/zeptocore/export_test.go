package zeptocore

import (
	"encoding/binary"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"testing"

	"github.com/schollz/_core/core/src/sox"
)

func TestExportPaddingAndCompanion(t *testing.T) {
	if _, err := exec.LookPath(sox.GetBinary()); err != nil {
		t.Skipf("SoX required: %v", err)
	}
	previousTempDir := sox.TempDir
	sox.TempDir = t.TempDir()
	t.Cleanup(func() { sox.TempDir = previousTempDir })
	for _, rate := range []int{44100, 88200} {
		for _, channels := range []int{1, 2} {
			t.Run(fmt.Sprintf("%dHz_%dch", rate, channels), func(t *testing.T) {
				folder := t.TempDir()
				frames := rate + 137
				frameBytes := channels * 2
				data := make([]byte, 44+frames*frameBytes)
				copy(data, "RIFF")
				binary.LittleEndian.PutUint32(data[4:], uint32(len(data)-8))
				copy(data[8:], "WAVEfmt ")
				binary.LittleEndian.PutUint32(data[16:], 16)
				binary.LittleEndian.PutUint16(data[20:], 1)
				binary.LittleEndian.PutUint16(data[22:], uint16(channels))
				binary.LittleEndian.PutUint32(data[24:], uint32(rate))
				binary.LittleEndian.PutUint32(data[28:], uint32(rate*frameBytes))
				binary.LittleEndian.PutUint16(data[32:], uint16(frameBytes))
				binary.LittleEndian.PutUint16(data[34:], 16)
				copy(data[36:], "data")
				binary.LittleEndian.PutUint32(data[40:], uint32(len(data)-44))
				for frame := 0; frame < frames; frame++ {
					for channel := 0; channel < channels; channel++ {
						value := int16(frame*12000/frames - 6000)
						binary.LittleEndian.PutUint16(data[44+(frame*channels+channel)*2:], uint16(value))
					}
				}
				source := filepath.Join(folder, "source.wav")
				if err := os.WriteFile(source, data, 0600); err != nil {
					t.Fatal(err)
				}
				for variant := 0; variant < 2; variant++ {
					output := filepath.Join(folder, fmt.Sprintf("0.%d.wav", variant))
					var err error
					if variant == 0 {
						err = processSound(source, output, channels, rate/44100)
					} else {
						err = createTimeStretched(source, output, 0.125, channels, rate/44100)
					}
					if err != nil {
						t.Fatal(err)
					}
					wav, err := os.ReadFile(output)
					if err != nil {
						t.Fatal(err)
					}
					bodyBytes := frames * frameBytes
					if variant == 1 {
						bodyBytes *= 8
					}
					padBytes := rate / 2 * frameBytes
					if len(wav) != 44+bodyBytes+2*padBytes || string(wav[36:40]) != "data" {
						t.Fatalf("variant %d: got %d bytes, want one WAV header + %d audio bytes + two %d-byte pads", variant, len(wav), bodyBytes, padBytes)
					}
					if gotChannels := int(binary.LittleEndian.Uint16(wav[22:])); gotChannels != channels {
						t.Fatalf("variant %d: WAV has %d channels, want %d", variant, gotChannels, channels)
					}
					f := File{Channels: channels - 1, Oversampling: rate / 44100, BPM: 120, SpliceTrigger: 24, Transients: [][]int{{}, {}, {}}}
					if err := f.updateInfo(output); err != nil {
						t.Fatal(err)
					}
					metadata, err := os.ReadFile(output + ".info")
					if err != nil {
						t.Fatal(err)
					}
					if got := int(binary.LittleEndian.Uint32(metadata)); got != bodyBytes {
						t.Fatalf("metadata body size %d, want %d", got, bodyBytes)
					}
					if variant == 1 && channels == 2 {
						// A legacy companion may claim mono while its metadata says stereo.
						broken := append([]byte(nil), wav...)
						binary.LittleEndian.PutUint16(broken[22:], 1)
						binary.LittleEndian.PutUint32(broken[28:], uint32(rate*2))
						binary.LittleEndian.PutUint16(broken[32:], 2)
						companion := filepath.Join(folder, "source.1.wav")
						if err := os.WriteFile(companion, broken, 0600); err != nil {
							t.Fatal(err)
						}
						if err := f.updateInfo(companion); err == nil {
							t.Fatal("mismatched WAV must not receive stereo metadata")
						}
						f.PathToAudio = source
						if err := f.repairCompanionFormat(); err != nil {
							t.Fatal(err)
						}
						repaired, err := os.ReadFile(companion)
						if err != nil || binary.LittleEndian.Uint16(repaired[22:]) != 2 || len(repaired) != len(wav) {
							t.Fatalf("companion recovery failed: %v", err)
						}
						metadata, err := os.ReadFile(companion + ".info")
						if err != nil {
							t.Fatal(err)
						}
						flags := binary.LittleEndian.Uint32(metadata[4:])
						binary.LittleEndian.PutUint32(metadata[4:], flags&^(1<<15))
						if err := os.WriteFile(companion+".info", metadata, 0600); err != nil {
							t.Fatal(err)
						}
						if err := f.repairCompanionFormat(); err != nil {
							t.Fatal(err)
						}
						metadata, err = os.ReadFile(companion + ".info")
						if err != nil || binary.LittleEndian.Uint32(metadata[4:])&(1<<15) == 0 {
							t.Fatalf("metadata-only recovery failed: %v", err)
						}
					}
					pcm := wav[44:]
					// Both circular pads must copy the opposite end of the body.
					// SoX dithering may differ by up to two PCM16 units per copy.
					for _, pair := range [][2]int{{0, bodyBytes}, {padBytes + bodyBytes, padBytes}} {
						for offset := 0; offset < padBytes; offset += 2 {
							a := int(int16(binary.LittleEndian.Uint16(pcm[pair[0]+offset:])))
							b := int(int16(binary.LittleEndian.Uint16(pcm[pair[1]+offset:])))
							if delta := a - b; delta < -2 || delta > 2 {
								t.Fatalf("variant %d: circular padding differs at byte %d: %d vs %d", variant, offset, a, b)
							}
						}
					}
				}
			})
		}
	}
}
