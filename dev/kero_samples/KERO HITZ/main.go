package main

import (
	"encoding/json"
	"fmt"
	"io"
	"math"
	"math/rand"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	log "github.com/schollz/logger"

	"github.com/schollz/_core/core/src/sox"
)

func randomString(n int) string {
	var letterRunes = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")
	b := make([]rune, n)
	for i := range b {
		b[i] = letterRunes[rand.Intn(len(letterRunes))]
	}
	return string(b)
}

func main() {
	log.SetLevel("info")
	// collect every wav file in the current folder
	files, err := filepath.Glob("*.wav")
	if err != nil {
		log.Error(err)
		return
	}

	kicks := []string{}
	snare := []string{}
	hats := []string{}
	other := []string{}
	for _, file := range files {
		duration, _ := sox.Length(file)
		if duration < 0.1 {
			continue
		}
		if strings.Contains(file, "_bd") {
			kicks = append(kicks, file)
		} else if strings.Contains(file, "_sd") {
			snare = append(snare, file)
		} else if strings.Contains(file, "snare") {
			snare = append(snare, file)
		} else if strings.Contains(file, "hh") {
			hats = append(hats, file)
		} else if strings.Contains(file, "hat") {
			hats = append(hats, file)
		} else {
			other = append(other, file)
		}
	}

	defer sox.Clean()
	// file, err := os.Open("2.ogg") // Open the OGG file
	// if err != nil {
	// 	log.Error(err)
	// }
	// defer file.Close()

	// metadata, err := tag.ReadFrom(file) // Read metadata from the file
	// if err != nil {
	// 	log.Error(err)
	// }
	// // ffmpeg -i TT_KERO_64syn.wav -metadata comment="helloworld" 2.ogg
	// fmt.Println("Comment:", metadata.Comment())

	fmt.Println(len(kicks), len(snare), len(hats), len(other))
	for bank := 0; bank < 2; bank++ {
		folder := fmt.Sprintf("folder%d", bank)
		os.MkdirAll(folder, 0755)
		for drumset := 0; drumset < 16; drumset++ {
			set := []string{}
			// add 4 kicks
			for i := 0; i < 4; i++ {
				set = append(set, kicks[rand.Intn(len(kicks))])
			}
			// add 2 snares
			for i := 0; i < 2; i++ {
				set = append(set, snare[rand.Intn(len(snare))])
			}
			// add 2 hats
			for i := 0; i < 2; i++ {
				set = append(set, hats[rand.Intn(len(hats))])
			}
			// add 8 others
			for i := 0; i < 8; i++ {
				set = append(set, other[rand.Intn(len(other))])
			}
			fmt.Printf("%v\n", set)

			// create a new file with spacing of 0.2 seconds between each
			filesToMerge := []string{}
			totalTime := 0.0
			sliceStart := []float64{}
			sliceStop := []float64{}
			for _, f := range set {
				silenceDuration := 0.2
				var newFilename string
				duration, _ := sox.Length(f)
				newFilename, err = sox.SilenceAppend(f, silenceDuration)
				if err != nil {
					log.Error(err)
					return
				}
				startTime := totalTime - 0.002
				if startTime < 0 {
					startTime = 0
				}
				sliceStart = append(sliceStart, totalTime)
				sliceStop = append(sliceStop, totalTime+duration+0.1)
				filesToMerge = append(filesToMerge, newFilename)
				totalTime += (duration + silenceDuration)
			}

			var mergedFile string
			mergedFile, err = sox.Join(filesToMerge...)
			if err != nil {
				log.Error(err)
				return
			}

			// round metadata to the nearest 0.001
			for i, v := range sliceStart {
				sliceStart[i] = float64(math.Round(v*1000)) / 1000
			}
			for i, v := range sliceStop {
				sliceStop[i] = float64(math.Round(v*1000)) / 1000
			}
			sliceStartBytes, _ := json.Marshal(sliceStart)
			sliceStopBytes, _ := json.Marshal(sliceStop)
			// create ogg from merged file
			oggFile := filepath.Join(folder, fmt.Sprintf("%s_%d.ogg", randomString(8), drumset))
			cmd := exec.Command("ffmpeg", "-i", mergedFile, "-metadata", "comment=oneshot", "-metadata", "artist="+string(sliceStartBytes), "-metadata", "album="+string(sliceStopBytes), oggFile)
			var out []byte
			out, err = cmd.CombinedOutput()
			if err != nil {
				log.Errorf("%s: %s", err, out)
				return
			}
		}
	}
}

// CopyFile copies a file from src to dst. If src and dst files exist, and are
// the same, then return success. Otherise, attempt to create a hard link
// between the two files. If that fail, copy the file contents from src to dst.
func CopyFile(src, dst string) (err error) {
	sfi, err := os.Stat(src)
	if err != nil {
		return
	}
	if !sfi.Mode().IsRegular() {
		// cannot copy non-regular files (e.g., directories,
		// symlinks, devices, etc.)
		return fmt.Errorf("CopyFile: non-regular source file %s (%q)", sfi.Name(), sfi.Mode().String())
	}
	dfi, err := os.Stat(dst)
	if err != nil {
		if !os.IsNotExist(err) {
			return
		}
	} else {
		if !(dfi.Mode().IsRegular()) {
			return fmt.Errorf("CopyFile: non-regular destination file %s (%q)", dfi.Name(), dfi.Mode().String())
		}
		if os.SameFile(sfi, dfi) {
			return
		}
	}
	if err = os.Link(src, dst); err == nil {
		return
	}
	err = copyFileContents(src, dst)
	return
}

// copyFileContents copies the contents of the file named src to the file named
// by dst. The file will be created if it does not already exist. If the
// destination file exists, all it's contents will be replaced by the contents
// of the source file.
func copyFileContents(src, dst string) (err error) {
	in, err := os.Open(src)
	if err != nil {
		return
	}
	defer in.Close()
	out, err := os.Create(dst)
	if err != nil {
		return
	}
	defer func() {
		cerr := out.Close()
		if err == nil {
			err = cerr
		}
	}()
	if _, err = io.Copy(out, in); err != nil {
		return
	}
	err = out.Sync()
	return
}
