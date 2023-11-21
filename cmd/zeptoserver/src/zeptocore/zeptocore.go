package zeptocore

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path"
	"path/filepath"

	log "github.com/schollz/logger"
)

type File struct {
	PathToFile   string
	BPM          int
	Beats        int
	SliceStart   []float64 // fractional (0-1)
	SliceStop    []float64 // fractional (0-1)
	Transposable bool
	OneShot      bool
	Stereo       bool // true if stereo
	Oversampling int  // 1, 2, or 4
}

func Get(pathToOriginal string) (f File, err error) {
	f = File{
		PathToFile: pathToOriginal,
	}
	if f.Load() == nil {
		return
	}
	// create new file
	f = File{
		PathToFile: pathToOriginal,
	}

	// TODO: load defaults

	// f.BPM, f.Beats, f.SliceStart, f.SliceStop, err = GetSliceMarkers(f.PathToFile)
	// if err != nil {
	// 	log.Error(err)
	// 	return
	// }
	// f.SliceNum = uint16(len(f.SliceStart))
	return
}

func (f File) Save() (err error) {
	fi, err := os.Create(fmt.Sprintf("%s.json", f.PathToFile))
	if err != nil {
		return
	}
	defer fi.Close()
	err = json.NewEncoder(fi).Encode(f)
	return
}

func (f *File) Load() (err error) {
	fi, err := os.Open(fmt.Sprintf("%s.json", f.PathToFile))
	if err != nil {
		return
	}
	defer fi.Close()
	err = json.NewDecoder(fi).Decode(&f)
	return
}

func (f *File) SetBPM(bpm int) {
	f.BPM = bpm
	go func() {
		f.Save()
	}()
}

func (f *File) SetBeats(beats int) {
	f.Beats = beats
	go func() {
		f.Save()
	}()
}

func (f *File) SetSlices(sliceStart []float64, sliceEnd []float64) {
	f.SliceStart = sliceStart
	f.SliceStop = sliceEnd
	go func() {
		f.Save()
	}()
}

func (f *File) SetOversampling(oversampling int) {
	f.Oversampling = oversampling
	go func() {
		f.Save()
	}()
}

func (f *File) SetStereo(stereo bool) {
	f.Stereo = stereo
	go func() {
		f.Save()
	}()
}

func (f *File) SetOneshot(oneshot bool) {
	f.OneShot = oneshot
	go func() {
		f.Save()
	}()
}

func (f *File) SetTransposable(transposable bool) {
	f.Transposable = transposable
	go func() {
		f.Save()
	}()
}

func (f File) updateFile(index int) (err error) {
	folder, PathToFile := filepath.Split(f.PathToFile)

	// set the number of channels
	channels := 1
	if f.Stereo {
		channels = 2
	}

	// determine the size
	finfo, err := os.Stat(f.PathToFile)
	if err != nil {
		log.Error(err)
		return
	}
	totalSamples := float64(finfo.Size()-44) / float64(channels) / 2
	totalSamples = totalSamples - 22050*2*float64(f.Oversampling)
	fsize := totalSamples * float64(channels) * 2 // total size excluding padding = totalSamples channels x 2 bytes
	sliceNum := len(f.SliceStart)
	if sliceNum == 0 {
		err = fmt.Errorf("no slices")
		return
	}
	slicesStart := []uint32{}
	slicesEnd := []uint32{}
	for i, _ := range f.SliceStart {
		slicesStart = append(slicesStart, uint32(math.Round(f.SliceStart[i]*fsize))/4*4)
		slicesEnd = append(slicesEnd, uint32(math.Round(f.SliceStop[i]*fsize))/4*4)
	}

	buf := new(bytes.Buffer)

	BPMTransposable := uint8(0)
	if f.Transposable {
		BPMTransposable = 1
	}

	StopCondition := uint8(0)
	if f.OneShot {
		StopCondition = 1
	}

	// file_list.h:
	// typedef struct WavFile {
	// 	uint32_t size;
	// 	char *name;
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
	var data = []any{
		uint16(len([]byte(PathToFile))),
		[]byte(PathToFile),
		uint32(fsize),
		uint16(f.BPM),
		uint16(f.Beats),
		sliceNum,
		slicesStart,
		slicesEnd,
		BPMTransposable,
		StopCondition,
		uint8(f.Oversampling),
		uint8(channels),
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
	fInfoWrite, _ := os.Create(path.Join(folder, fmt.Sprintf("%s.info.%d", PathToFile, index)))
	fInfoWrite.Write(buf2.Bytes())
	fInfoWrite.Close()
	return
}
