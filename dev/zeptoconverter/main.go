package main

// A simple example that shows how to render an animated progress bar. In this
// example we bump the progress by 25% every two seconds, animating our
// progress bar to its new target state.
//
// It's also possible to render a progress bar in a more static fashion without
// transitions. For details on that approach see the progress-static example.

import (
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"

	log "github.com/schollz/logger"
	"github.com/schollz/progressbar/v3"
	"github.com/schollz/sox"
)

var flagFolderIn, flagFolderOut string
var flagStereo bool

// file_list.h:
// typedef struct WavFile {
// 	char *name;
// 	uint32_t size;
// 	uint16_t bpm;
// 	uint16_t beats;
// 	uint8_t slice_num;
// 	uint32_t *slice_start;
// 	uint32_t *slice_end;
// 	uint8_t bpm_transposable;
// 	uint8_t stop_condition;
// } WavFile;

type WavFile struct {
	NameLen         uint16
	Name            []byte   `c:"name"`
	Size            uint32   `c:"size"`
	BPM             uint16   `c:"bpm"`
	Beats           uint16   `c:"beats"`
	SliceNum        uint16   `c:"slice_num"`
	SliceStart      []uint32 `c:"slice_val"`
	SliceStop       []uint32
	BPMTransposable uint8 `c:"bpm_transposable"`
	StopCondition   uint8 `c:"stop_condition"`
}

func init() {
	flag.StringVar(&flagFolderIn, "in", "", "folder for input")
	flag.StringVar(&flagFolderOut, "out", "", "folder for output")
	flag.BoolVar(&flagStereo, "stereo", false, "stereo")
}

func main() {
	flag.Parse()
	log.SetLevel("debug")

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
	sort.Strings(fileList)

	bar := progressbar.Default(int64(len(fileList)))
	for _, f := range fileList {
		fpath, filename := filepath.Split(f)
		fpath, _ = filepath.Abs(fpath)
		fpathRelative := strings.TrimPrefix(fpath, flagFolderIn)
		if len(fpathRelative) > 0 {
			if string(fpathRelative[0]) == "/" || string(fpathRelative[1]) == "\\" {
				fpathRelative = fpathRelative[1:]
			}
		}
		log.Debugf("fpath: %s", fpath)
		log.Debugf("filename: %s", filename)
		log.Debugf("fpathRelative: %s", fpathRelative)

		os.MkdirAll(path.Join(flagFolderOut, fpathRelative), os.ModePerm)

		channels := 1
		if flagStereo {
			channels = 2
		}

		filenameExt := filepath.Ext(filename)
		newExt := ".mono.wav"
		if channels == 2 {
			newExt = ".stereo.wav"
		}
		filename = filename[:len(filename)-len(filenameExt)] + newExt
		filenameSD := path.Join(fpathRelative, filename)

		beats, bpm, err := processSound0(f, path.Join(flagFolderOut, filenameSD), channels)
		if err != nil {
			log.Error(err)
			return
		}
		err = processInfo0(filenameSD, beats, bpm, channels)
		if err != nil {
			log.Error(err)
			return
		}
		bar.Add(1)
	}
}

func processInfo0(filenameSD string, beats float64, bpm float64, channels int) (err error) {
	finfo, err := os.Stat(path.Join(flagFolderOut, filenameSD))
	if err != nil {
		log.Error(err)
		return
	}
	totalSamples := float64(finfo.Size()-44) / float64(channels) / 2
	totalSamples = totalSamples - 22050*2         // total samples excluding padding = each side is padded with extra samples
	fsize := totalSamples * float64(channels) * 2 // total size excluding padding = totalSamples channels x 2 bytes
	slices := []uint32{}
	slicesEnd := []uint32{}
	sliceNum := uint16(beats * 2)
	for i := 0.0; i < beats*2; i++ {
		slices = append(slices, uint32(math.Round(fsize*i/float64(sliceNum)))/4*4)
		slicesEnd = append(slicesEnd, uint32(math.Round(fsize*(i+1)/float64(sliceNum)))/4*4)
	}
	wav1 := WavFile{
		NameLen:         uint16(len([]byte(filenameSD))),
		Name:            []byte(filenameSD),
		Size:            uint32(fsize),
		BPM:             uint16(bpm),
		Beats:           uint16(beats),
		SliceNum:        sliceNum,
		SliceStart:      slices,
		SliceStop:       slicesEnd,
		BPMTransposable: 0,
		StopCondition:   0,
	}
	err = writeWav(filenameSD, wav1)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func processSound0(fnameIn string, fnameOut string, channels int) (beats float64, bpm float64, err error) {
	beats, bpm, err = sox.GetBPM(fnameIn)
	if err != nil {
		log.Error(err)
		return
	}
	pieceFront, err := sox.Trim(fnameIn, 0, 0.5)
	if err != nil {
		log.Error(err)
		return
	}
	seconds, err := sox.Length(fnameIn)
	if err != nil {
		log.Error(err)
		return
	}
	pieceEnd, err := sox.Trim(fnameIn, seconds-0.5)
	if err != nil {
		log.Error(err)
		return
	}
	pieceJoin, err := sox.Join(pieceEnd, fnameIn, pieceFront)
	defer func() {
		os.Remove(pieceFront)
		os.Remove(pieceEnd)
		os.Remove(pieceJoin)
	}()

	_, _, err = run("sox", pieceJoin, "-c", fmt.Sprint(channels), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw")
	if err != nil {
		log.Error(err)
		return
	}
	_, _, err = run("sox", "-t", "raw", "-c", fmt.Sprint(channels), "-r", "44100", "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", fnameOut)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func writeWav(filenameSD string, wav1 WavFile) (err error) {
	log.Debugf("wav: %+v", wav1)
	buf := new(bytes.Buffer)
	var data = []any{
		wav1.NameLen,
		wav1.Name,
		wav1.Size,
		wav1.BPM,
		wav1.Beats,
		wav1.SliceNum,
		wav1.SliceStart,
		wav1.SliceStop,
		wav1.BPMTransposable,
		wav1.StopCondition,
	}
	for _, v := range data {
		err := binary.Write(buf, binary.LittleEndian, v)
		if err != nil {
			log.Errorf("binary.Write failed: %s", err.Error())
		}
	}
	// prepend with the total size
	data = append([]any{uint16(buf.Len())}, data...)
	buf2 := new(bytes.Buffer)
	for _, v := range data {
		err := binary.Write(buf2, binary.LittleEndian, v)
		if err != nil {
			log.Errorf("binary.Write failed: %s", err.Error())
		}
	}
	fInfoWrite, _ := os.Create(path.Join(flagFolderOut, filenameSD+".info"))
	fInfoWrite.Write(buf2.Bytes())
	fInfoWrite.Close()

	return
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
