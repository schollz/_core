package main

import (
	"fmt"
	"io"
	"math/rand"
	"os"
	"path/filepath"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
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

	filesToUse := []string{}
	for _, file := range files {
		duration, _ := sox.Length(file)
		if duration > 3 {
			filesToUse = append(filesToUse, file)
		}
	}
	fmt.Println(len(filesToUse))

	// reorder them randomly
	for i := range filesToUse {
		j := rand.Intn(i + 1)
		filesToUse[i], filesToUse[j] = filesToUse[j], filesToUse[i]
	}

	// split files into 16 folders
	for i, file := range filesToUse {
		folder := i / 16
		newFile := fmt.Sprintf("folder%d/%s", folder, randomString(6)+".wav")
		log.Infof("Moving %s to %s", file, newFile)
		// make folder if it doesn't exist
		os.MkdirAll(fmt.Sprintf("folder%d", folder), 0755)
		err := CopyFile(file, newFile)
		if err != nil {
			log.Error(err)
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
