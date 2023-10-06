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
	"io"
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
		f0 := path.Join("/tmp/", strings.Replace(filename, " ", "_", -1))
		f0 = strings.Replace(f0, "(", "_", -1)
		f0 = strings.Replace(f0, ")", "_", -1)
		CopyFile(f, f0)
		f = f0
		beats, bpm, _ := sox.GetBPM(f)
		f2 := path.Join(flagFolderOut, pathRelative, filename)
		ext := path.Ext(f2)
		final1 := f2[0:len(f2)-len(ext)] + fmt.Sprintf("_bpm%d_beats%d.wav", int(bpm), int(beats))
		final2 := f2[0:len(f2)-len(ext)] + fmt.Sprintf("_bpm%d_beats%d_timestretch.wav", int(bpm), int(beats*4))
		bar.Add(1)
		run("sox", f, "-c", "1", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw")
		run("sox", "-t", "raw", "-r", "44100", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", final1)
		run("rubberband", "-2", "-t4", f, "/tmp/1.wav")
		run("sox", "/tmp/1.wav", "-c", "1", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw")
		run("sox", "-t", "raw", "-r", "44100", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", final2)
		os.Remove(f)
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
