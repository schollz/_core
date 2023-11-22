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
	"sync"
	"time"

	"github.com/bep/debounce"
	log "github.com/schollz/logger"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/op1"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/renoise"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/sox"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/utils"
)

type File struct {
	Filename     string
	Duration     float64
	PathToFile   string
	PathToAudio  string
	BPM          int
	SliceStart   []float64 // fractional (0-1)
	SliceStop    []float64 // fractional (0-1)
	TempoMatch   bool
	OneShot      bool
	Channels     int // 1 if mono, 2 if stereo
	Oversampling int // 1, 2, or 4

	debounceSave  func(f func())
	debounceRegen func(f func())
}

var (
	mu      sync.Mutex
	working int
)

func Get(pathToOriginal string) (f File, err error) {
	_, filename := filepath.Split(pathToOriginal)
	f = File{
		Filename:   filename,
		PathToFile: pathToOriginal,
	}
	if f.Load() == nil {
		log.Debugf("loaded %s from disk", pathToOriginal)
		f.debounceSave = debounce.New(1 * time.Second)
		f.debounceRegen = debounce.New(1 * time.Second)
		return
	}
	log.Debugf("creating new %s, could not find cache", pathToOriginal)
	// create new file
	f = File{
		Filename:      filename,
		PathToFile:    pathToOriginal,
		PathToAudio:   pathToOriginal,
		debounceSave:  debounce.New(1 * time.Second),
		debounceRegen: debounce.New(1 * time.Second),
		OneShot:       false,
		TempoMatch:    true,
		Channels:      1,
		Oversampling:  1,
	}
	var errSliceDetect error
	if filepath.Ext(f.PathToFile) == ".xrni" {
		var newPath string
		log.Tracef("opening renoise %s", f.PathToFile)
		newPath, f.SliceStart, f.SliceStop, errSliceDetect = renoise.GetSliceMarkers(f.PathToFile)
		if errSliceDetect == nil {
			f.PathToAudio = newPath
		}
	} else if filepath.Ext(f.PathToFile) == ".aif" {
		log.Tracef("attempting op1 %s", f.PathToFile)
		f.SliceStart, f.SliceStop, errSliceDetect = op1.GetSliceMarkers(f.PathToFile)
	}
	// determine the duration
	log.Tracef("determining the duration of %s", f.PathToAudio)
	f.Duration, err = sox.Length(f.PathToAudio)
	if err != nil {
		log.Error(err)
		return
	}
	var beats float64
	var bpm float64
	beats, bpm, err = sox.GetBPM(f.PathToAudio)
	f.BPM = int(math.Round(bpm))
	if len(f.SliceStart) == 0 || errSliceDetect != nil {
		// determine programmatically
		slices := beats * 2
		f.SliceStart = make([]float64, int(slices))
		f.SliceStop = make([]float64, int(slices))
		for i := 0; i < int(slices); i++ {
			f.SliceStart[i] = float64(i) / slices
			f.SliceStop[i] = float64(i+1) / slices
		}
	}

	// get the folder of the original flie
	folder, filename := filepath.Split(f.PathToFile)
	// remove extension from file name
	filenameWithouExt := filename[:len(filename)-len(filepath.Ext(filename))]

	// create the 0 file (original)
	fname0 := path.Join(folder, fmt.Sprintf("%s.0.wav", filenameWithouExt))

	// check if fname0 exists
	if _, err := os.Stat(fname0); err != nil {
		// save the json
		f.Save()

		// regenerate the audio
		f.Regenerate()

		// create mp3
		_, _, err = utils.Run("sox", f.PathToAudio, f.PathToFile+".mp3")
		if err != nil {
			log.Error(err)
		}
	}
	return
}

func (f File) Save() (err error) {
	fu := func() {
		log.Tracef("writing %s.json", f.PathToFile)
		fi, err := os.Create(fmt.Sprintf("%s.json", f.PathToFile))
		if err != nil {
			log.Error(err)
			return
		}
		defer fi.Close()

		err = json.NewEncoder(fi).Encode(f)
		if err != nil {
			log.Error(err)
		}
	}
	f.debounceSave(fu)
	return
}

func IsBusy() bool {
	return working > 0
}

func (f File) Regenerate() {
	fu := func() {
		mu.Lock()
		working++
		mu.Unlock()
		defer func() {
			mu.Lock()
			working--
			mu.Unlock()
		}()
		log.Tracef("regenerating %s", f.PathToFile)
		// get the folder of the original flie
		folder, filename := filepath.Split(f.PathToFile)
		// remove extension from file name
		filenameWithouExt := filename[:len(filename)-len(filepath.Ext(filename))]

		// create the 0 file (original)
		fname0 := path.Join(folder, fmt.Sprintf("%s.0.wav", filenameWithouExt))
		err := processSound(f.PathToAudio, fname0, f.Channels, f.Oversampling)
		if err != nil {
			log.Error(err)
		}
		err = f.updateInfo(fname0)
		if err != nil {
			log.Error(err)
		}

		fname1 := path.Join(folder, fmt.Sprintf("%s.1.wav", filenameWithouExt))
		err = createTimeStretched(f.PathToAudio, fname1, 0.5, f.Channels, f.Oversampling)
		if err != nil {
			log.Error(err)
		}
		err = f.updateInfo(fname1)
		if err != nil {
			log.Error(err)
		}

	}
	f.debounceRegen(fu)
}

func (f *File) Load() (err error) {
	fi, err := os.Open(fmt.Sprintf("%s.json", f.PathToFile))
	if err != nil {
		log.Trace(err)
		return
	}
	defer fi.Close()
	err = json.NewDecoder(fi).Decode(&f)
	if err != nil {
		log.Error(err)
	}
	return
}

func (f *File) SetBPM(bpm int) {
	f.BPM = bpm
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
	different := f.Oversampling != oversampling
	f.Oversampling = oversampling
	go func() {
		f.Save()
	}()
	if different {
		go func() {
			f.Regenerate()
		}()
	}
}

func (f *File) SetChannels(channels int) {
	different := f.Channels != channels
	f.Channels = channels
	go func() {
		f.Save()
	}()
	if different {
		go func() {
			f.Regenerate()
		}()
	}
}

func (f *File) SetOneshot(oneshot bool) {
	f.OneShot = oneshot
	go func() {
		f.Save()
	}()
}

func (f *File) SetTempoMatch(TempoMatch bool) {
	f.TempoMatch = TempoMatch
	go func() {
		f.Save()
	}()
}

// createTimeStretched will create timestretched file from input
// and process it to format it for zeptocore
func createTimeStretched(fnameIn string, fnameOut string, ratio float64, channels int, oversampling int) (err error) {
	log.Tracef("creating timestretched %s", fnameOut)
	_, _, err = utils.Run("sox", fnameIn, "1.wav", "tempo", "-m", fmt.Sprintf("%2.8f", ratio))
	if err != nil {
		log.Error(err)
		return
	}
	err = processSound("1.wav", fnameOut, channels, oversampling)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

// processSound takes a sound file and processes it to be ready for the zeptocore
// by padding the beginning with the end and the end with the beginning
// as well as converting it to the right format and bit rate
func processSound(fnameIn string, fnameOut string, channels int, oversampling int) (err error) {
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

	_, _, err = utils.Run("sox", pieceJoin, "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", "norm", "gain", "-1")
	if err != nil {
		log.Error(err)
		return
	}
	_, _, err = utils.Run("sox", "-t", "raw", "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", fnameOut)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func (f File) updateInfo(fnameIn string) (err error) {
	_, filename := filepath.Split(fnameIn)

	// determine the size
	finfo, err := os.Stat(fnameIn)
	if err != nil {
		log.Error(err)
		return
	}
	totalSamples := float64(finfo.Size()-44) / float64(f.Channels) / 2
	totalSamples = totalSamples - 22050*2*float64(f.Oversampling)
	fsize := totalSamples * float64(f.Channels) * 2 // total size excluding padding = totalSamples channels x 2 bytes
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

	BPMTempoMatch := uint8(0)
	if f.TempoMatch {
		BPMTempoMatch = 1
	}

	StopCondition := uint8(0)
	if f.OneShot {
		StopCondition = 2
	}

	// file_list.h:
	// typedef struct WavFile {
	// 	uint32_t size;
	// 	char *name;
	// 	uint16_t bpm;
	// 	uint8_t slice_num;
	// 	uint32_t *slice_start;
	// 	uint32_t *slice_end;
	// 	uint8_t bpm_TempoMatch;
	// 	uint8_t stop_condition;
	//  uint8_t oversampling;
	//  uint8_t num_channels;
	// } WavFile;
	var data = []any{
		uint16(len([]byte(filename))),
		[]byte(filename),
		uint32(fsize),
		uint16(f.BPM),
		uint8(sliceNum),
		slicesStart,
		slicesEnd,
		uint8(BPMTempoMatch),
		uint8(StopCondition),
		uint8(f.Oversampling),
		uint8(f.Channels),
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
	fInfoWrite, _ := os.Create(fnameIn + ".info")
	fInfoWrite.Write(buf2.Bytes())
	fInfoWrite.Close()
	return
}
