package convert

import (
	"bytes"
	"encoding/binary"
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
	"github.com/schollz/zeptoconverter/lib/op1"
	"github.com/schollz/zeptoconverter/lib/renoise"
	"github.com/schollz/zeptoconverter/lib/sox"
)

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
//  uint8_t oversampling;
//  uint8_t num_channels;
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
	Oversampling    uint8
	NumChannels     uint8
}

type Converter struct {
	FolderIn     string
	FolderOut    string
	Stereo       bool
	Oversampling int
}

func Convert(c Converter) (err error) {
	c.FolderIn, _ = filepath.Abs(c.FolderIn)
	folderCount := 0
	fileCount := 0
	fileList := []string{}
	filepath.Walk(c.FolderIn, func(fpath string, info os.FileInfo, err error) error {
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
		fpathRelative := strings.TrimPrefix(fpath, c.FolderIn)
		if len(fpathRelative) > 0 {
			if string(fpathRelative[0]) == "/" || string(fpathRelative[1]) == "\\" {
				fpathRelative = fpathRelative[1:]
			}
		}

		var slicesStart []int
		var slicesEnd []int
		if filepath.Ext(f) == ".xrni" {
			f, slicesStart, slicesEnd, err = renoise.GetSliceMarkers(f)
			_, filename = filepath.Split(f)
			if err != nil {
				log.Error(err)
				continue
			}
		} else if filepath.Ext(f) == ".aif" {
			slicesStart, slicesEnd, err = op1.GetSliceMarkers(f)
			if err != nil {
				slicesStart = []int{}
				slicesEnd = []int{}
			}
		}

		log.Debugf("fpath: %s", fpath)
		log.Debugf("filename: %s", filename)
		log.Debugf("fpathRelative: %s", fpathRelative)

		os.MkdirAll(path.Join(c.FolderOut, fpathRelative), os.ModePerm)

		channels := 1
		if c.Stereo {
			channels = 2
		}

		filenameExt := filepath.Ext(filename)
		newExt := ".mono.wav"
		if channels == 2 {
			newExt = ".stereo.wav"
		}
		filename = filename[:len(filename)-len(filenameExt)] + newExt
		filenameSD := path.Join(fpathRelative, filename)

		var beats float64
		var bpm float64
		beats, bpm, err = c.processSound(f, path.Join(c.FolderOut, filenameSD), channels)
		if err != nil {
			log.Error(err)
			return
		}
		err = c.processInfo(filenameSD, fmt.Sprintf("%s.info", filenameSD), beats, bpm, channels, slicesStart, slicesEnd)
		if err != nil {
			log.Error(err)
			return
		}

		err = c.createTimeStretched(f, filenameSD, beats, bpm, channels, slicesStart, slicesEnd, 4)
		if err != nil {
			log.Error(err)
			return
		}

		bar.Add(1)
	}
	return
}

func (c Converter) createTimeStretched(fnameIn string, filenameSD string, beats float64, bpm float64, channels int, slicesStart []int, slicesEnd []int, count int) (err error) {
	// ratios := []float64{0.89089871814075, 0.79370052598483, 0.74915353843921, 0.6674199270861, 0.5946035575026, 0.52973154718099, 0.5, 0.44, .37, 0.25, 0.1}
	ratios := []float64{0.5, 0.25, 0.1}
	for i, ratio := range ratios {
		_, _, err = run("sox", fnameIn, "1.wav", "tempo", "-m", fmt.Sprintf("%2.8f", ratio))
		if err != nil {
			log.Error(err)
			return
		}
		fname := fmt.Sprintf("%s.%d.wav", filenameSD, i+1)
		log.Debugf("creating timestretched %s", fname)
		_, _, err = c.processSound("1.wav", path.Join(c.FolderOut, fname), channels)
		if err != nil {
			log.Error(err)
			return
		}
		log.Debugf("created timestretched file: '%s'", fname)
		// redo the slices according to the ratio
		slicesStartFile := make([]int, len(slicesStart))
		slicesEndFile := make([]int, len(slicesEnd))
		if len(slicesStart) > 0 {
			for j, v := range slicesStart {
				slicesStartFile[j] = int(math.Round(float64(v) / ratio))
			}
			for j, v := range slicesEnd {
				slicesEndFile[j] = int(math.Round(float64(v) / ratio))
			}
		}
		err = c.processInfo(fname, fmt.Sprintf("%s.info.%d", filenameSD, i+1), beats, bpm, channels, slicesStartFile, slicesEndFile)
		if err != nil {
			log.Error(err)
			return
		}
	}
	return
}

func (c Converter) processInfo(filenameSD string, filenameInfo string, beats float64, bpm float64, channels int, slicesStartFile []int, slicesEndFile []int) (err error) {
	finfo, err := os.Stat(path.Join(c.FolderOut, filenameSD))
	if err != nil {
		log.Error(err)
		return
	}
	totalSamples := float64(finfo.Size()-44) / float64(channels) / 2
	totalSamples = totalSamples - 22050*2*float64(c.Oversampling) // total samples excluding padding = each side is padded with extra samples
	fsize := totalSamples * float64(channels) * 2                 // total size excluding padding = totalSamples channels x 2 bytes
	sliceNum := uint16(beats * 2)
	slicesStart := []uint32{}
	slicesEnd := []uint32{}
	for i := 0.0; i < beats*2; i++ {
		slicesStart = append(slicesStart, uint32(math.Round(fsize*i/float64(sliceNum)))/4*4)
		slicesEnd = append(slicesEnd, uint32(math.Round(fsize*(i+1)/float64(sliceNum)))/4*4)
	}
	if len(slicesStartFile) > 0 {
		sliceNum = uint16(len(slicesStartFile))
		slicesStart = []uint32{}
		slicesEnd = []uint32{}
		for i, _ := range slicesStartFile {
			slicesStart = append(slicesStart, uint32(math.Round(float64(slicesStartFile[i])*float64(channels)*2/4*4)))
			slicesEnd = append(slicesEnd, uint32(math.Round(float64(slicesEndFile[i])*float64(channels)*2/4*4)))
		}
	}
	wav1 := WavFile{
		NameLen:         uint16(len([]byte(filenameSD))),
		Name:            []byte(filenameSD),
		Size:            uint32(fsize),
		BPM:             uint16(bpm),
		Beats:           uint16(beats),
		SliceNum:        sliceNum,
		SliceStart:      slicesStart,
		SliceStop:       slicesEnd,
		BPMTransposable: 0,
		StopCondition:   0,
		Oversampling:    uint8(c.Oversampling),
		NumChannels:     uint8(channels),
	}
	err = c.writeWav(filenameSD, filenameInfo, wav1)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func (c Converter) processSound(fnameIn string, fnameOut string, channels int) (beats float64, bpm float64, err error) {
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

	_, _, err = run("sox", pieceJoin, "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*c.Oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", "norm", "gain", "-1")
	if err != nil {
		log.Error(err)
		return
	}
	_, _, err = run("sox", "-t", "raw", "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*c.Oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", fnameOut)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func (c Converter) writeWav(filenameSD string, filenameInfo string, wav1 WavFile) (err error) {
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
		wav1.Oversampling,
		wav1.NumChannels,
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
	fInfoWrite, _ := os.Create(path.Join(c.FolderOut, filenameInfo))
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
