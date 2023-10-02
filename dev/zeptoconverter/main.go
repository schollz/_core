package main

// A simple example that shows how to render an animated progress bar. In this
// example we bump the progress by 25% every two seconds, animating our
// progress bar to its new target state.
//
// It's also possible to render a progress bar in a more static fashion without
// transitions. For details on that approach see the progress-static example.

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"

	log "github.com/schollz/logger"
	"github.com/schollz/progressbar/v3"
	"github.com/schollz/sox"
)

var flagFolderIn, flagFolderOut string

func init() {
	flag.StringVar(&flagFolderIn, "in", "", "folder for input")
	flag.StringVar(&flagFolderOut, "out", "", "folder for output")
}

func main() {
	flag.Parse()
	log.SetLevel("info")

	flagFolderIn, _ = filepath.Abs(flagFolderIn)
	fmt.Println(flagFolderIn)
	folderCount := 0
	fileCount := 0
	fileList := []string{}
	filepath.Walk(flagFolderIn, func(fpath string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			folderCount++
		} else {
			fileCount++
			fileList = append(fileList, fpath)
		}
		return nil
	})
	fmt.Println(fileList[0])

	bar := progressbar.Default(int64(len(fileList)))
	for _, f := range fileList {
		fpath, filename := filepath.Split(f)
		fpath, _ = filepath.Abs(fpath)
		pathRelative := strings.TrimPrefix(fpath, flagFolderIn)
		if len(pathRelative) > 0 {
			if string(pathRelative[0]) == "/" || string(pathRelative[1]) == "\\" {
				pathRelative = pathRelative[1:]
			}
		}
		os.MkdirAll(path.Join(flagFolderOut, pathRelative), os.ModePerm)
		beats, bpm, _ := sox.GetBPM(f)
		f2 := path.Join(flagFolderOut, pathRelative, filename)
		ext := path.Ext(f2)
		f2 = f2[0:len(f2)-len(ext)] + fmt.Sprintf("_bpm%d_beats%d.wav", int(beats), int(bpm))
		bar.Add(1)
		run("sox", f, "-c", "1", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", f2)
	}
}

func run(args ...string) (string, string, error) {
	log.Trace(strings.Join(args, " "))
	baseCmd := args[0]
	cmdArgs := args[1:]
	cmd := exec.Command(baseCmd, cmdArgs...)
	var outb, errb bytes.Buffer
	cmd.Stdout = &outb
	cmd.Stderr = &errb
	err := cmd.Run()
	if err != nil {
		log.Errorf("%s -> '%s'", strings.Join(args, " "), err.Error())
		log.Error(outb.String())
		log.Error(errb.String())
	}
	return outb.String(), errb.String(), err
}
