package zeptocore

/*
#cgo CFLAGS: -I.
#include "../../../lib/sampleinfo.h"
*/
import "C"
import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path"
	"path/filepath"
	"sync"
	"time"
	"unsafe"

	"github.com/bep/debounce"
	"github.com/schollz/_core/core/src/kickextract"
	"github.com/schollz/_core/core/src/onsetdetect"
	"github.com/schollz/_core/core/src/op1"
	"github.com/schollz/_core/core/src/renoise"
	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
)

type File struct {
	Filename      string
	Duration      float64
	PathToFile    string
	PathToAudio   string
	BPM           int
	SliceStart    []float64 // fractional (0-1)
	SliceStop     []float64 // fractional (0-1)
	SliceType     []int     // 0 = normal, 1 = kick
	TempoMatch    bool
	OneShot       bool
	Channels      int // 0 if mono, 1 if stereo
	Oversampling  int // 1, 2, or 4
	SpliceTrigger int // 0, 16, 32, 48, 64, 80, 96, 112, 128
	// from audio_callaback.h:
	// // starts at splice start and ends at splice stop
	// #define PLAY_SPLICE_STOP 0
	// // starts at splice start, and returns to start when reaching splice boundary
	// #define PLAY_SPLICE_LOOP 1
	// // starts at splice start and ends at sample boundary
	// #define PLAY_SAMPLE_STOP 2
	// // starts at splice start and returns to start when reaching sample boundary
	// #define PLAY_SAMPLE_LOOP 3
	SplicePlayback int

	debounceSave  func(f func())
	debounceRegen func(f func())
}

var (
	mu        sync.Mutex
	regenLock sync.Mutex
	working   int
)

func Get(pathToOriginal string) (f File, err error) {
	_, filename := filepath.Split(pathToOriginal)
	f = File{
		Filename:   filename,
		PathToFile: pathToOriginal,
	}
	if f.Load() == nil {
		log.Debugf("loaded %s from disk", pathToOriginal)
		f.debounceSave = debounce.New(100 * time.Millisecond)
		f.debounceRegen = debounce.New(100 * time.Millisecond)
		return
	}
	log.Debugf("creating new %s, could not find cache", pathToOriginal)
	// get the number of channels
	_, channels, _, _ := sox.Info(pathToOriginal)
	channels = channels - 1
	if channels < 0 {
		channels = 0
	} else if channels > 1 {
		channels = 1
	}
	// create new file
	f = File{
		Filename:      filename,
		PathToFile:    pathToOriginal,
		PathToAudio:   pathToOriginal,
		debounceSave:  debounce.New(100 * time.Millisecond),
		debounceRegen: debounce.New(100 * time.Millisecond),
		OneShot:       false,
		TempoMatch:    true,
		Channels:      channels,
		Oversampling:  1,
	}
	var errSliceDetect error
	errSliceDetect = fmt.Errorf("slice detection failed")
	if filepath.Ext(f.PathToFile) == ".xrni" {
		var newPath string
		log.Tracef("opening renoise %s", f.PathToFile)
		newPath, f.SliceStart, f.SliceStop, errSliceDetect = renoise.GetSliceMarkers(f.PathToFile)
		if errSliceDetect == nil {
			log.Trace("detected slices from renoise")
			f.PathToAudio = newPath
		}
	} else if filepath.Ext(f.PathToFile) == ".aif" {
		log.Tracef("attempting op1 %s", f.PathToFile)
		f.SliceStart, f.SliceStop, errSliceDetect = op1.GetSliceMarkers(f.PathToFile)
		if errSliceDetect == nil {
			log.Trace("detected slices from aif")
		} else {
			// try again using metadata
			var metad Metadata
			metad, errSliceDetect = GetMetadata(f.PathToFile)
			if errSliceDetect == nil {
				log.Trace("detected slices from metadata")
				f.SliceStart = metad.SliceStart
				f.SliceStop = metad.SliceStop
			}
		}
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
		if f.Duration > 2 {
			// determine programmatically
			slices := beats * 2
			f.SliceStart = make([]float64, int(slices))
			f.SliceStop = make([]float64, int(slices))
			for i := 0; i < int(slices); i++ {
				f.SliceStart[i] = float64(i) / slices
				f.SliceStop[i] = float64(i+1) / slices
			}

		} else {
			onsets, _ := onsetdetect.OnsetDetect(f.PathToAudio, 16)
			if len(onsets) > 0 {
				f.SliceStart = []float64{onsets[0]}
			} else {
				f.SliceStart = []float64{0.0}
			}
			f.SliceStop = []float64{1.0}
		}
	}

	f.SliceType = make([]int, len(f.SliceStart))
	f.UpdateSliceTypes()

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

		// create ogg
		_, _, err = utils.Run(sox.GetBinary(), f.PathToAudio, f.PathToFile+".ogg")
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
		regenLock.Lock()
		defer func() {
			regenLock.Unlock()
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
		err := processSound(f.PathToAudio, fname0, f.Channels+1, f.Oversampling)
		if err != nil {
			log.Errorf("could not process sound: %s %s", f.PathToAudio, err.Error())
			return
		}

		err = f.updateInfo(fname0)
		if err != nil {
			log.Error(err)
			return
		}

		log.Tracef("slices: %+v", f.SliceStart)
		fname1 := path.Join(folder, fmt.Sprintf("%s.1.wav", filenameWithouExt))
		err = createTimeStretched(f.PathToAudio, fname1, 0.125, f.Channels+1, f.Oversampling)
		if err != nil {
			log.Error(err)
		}
		log.Trace("-------------------------")
		log.Tracef("slices: %+v", f.SliceStart)
		log.Tracef("slice types: %+v", f.SliceType)
		log.Trace("-------------------------")
		err = f.updateInfo(fname1)
		if err != nil {
			log.Error(err)
		}

		// fname2 := path.Join(folder, fmt.Sprintf("%s.2.wav", filenameWithouExt))
		// err = createTimeStretched(f.PathToAudio, fname2, 0.125, f.Channels, f.Oversampling)
		// if err != nil {
		// 	log.Error(err)
		// }
		// err = f.updateInfo(fname2)
		// if err != nil {
		// 	log.Error(err)
		// }

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
		f.Regenerate()
	}()
}

func (f *File) SetSplicePlayback(playback int) {
	different := f.SplicePlayback != playback
	f.SplicePlayback = playback
	go func() {
		f.Save()
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) UpdateSliceTypes() {
	// calculate the splice type
	kicks, err := kickextract.KickExtract(f.PathToFile, f.SliceStart, f.SliceStop)
	if err != nil {
		log.Error(err)
	}
	f.SliceType = make([]int, len(kicks))
	for i := range kicks {
		if kicks[i] {
			f.SliceType[i] = 1
		}
	}
	log.Tracef("slice types: %+v", f.SliceType)
}

func (f *File) SetSlices(sliceStart []float64, sliceEnd []float64) (sliceType []int) {
	f.SliceStart = sliceStart
	f.SliceStop = sliceEnd
	f.UpdateSliceTypes()
	go func() {
		f.Save()
		f.Regenerate()
	}()
	return f.SliceType
}

func (f *File) SetOversampling(oversampling int) {
	different := f.Oversampling != oversampling
	f.Oversampling = oversampling
	go func() {
		f.Save()
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetChannels(channels int) {
	if channels < 0 {
		channels = 0
	} else if channels > 1 {
		channels = 1
	}
	different := f.Channels != channels
	f.Channels = channels
	go func() {
		f.Save()
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetOneshot(oneshot bool) {
	different := f.OneShot != oneshot
	f.OneShot = oneshot
	go func() {
		f.Save()
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetTempoMatch(TempoMatch bool) {
	different := f.TempoMatch != TempoMatch
	f.TempoMatch = TempoMatch
	go func() {
		f.Save()
		if different {
			f.Regenerate()
		}
	}()
}

// createTimeStretched will create timestretched file from input
// and process it to format it for zeptocore
func createTimeStretched(fnameIn string, fnameOut string, ratio float64, channels int, oversampling int) (err error) {
	log.Tracef("creating timestretched %s", fnameOut)
	_, _, err = utils.Run(sox.GetBinary(), fnameIn, "1.wav", "tempo", "-m", fmt.Sprintf("%2.8f", ratio))
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
		log.Errorf("could not trim %s: %s", fnameIn, err.Error())
		return
	}
	seconds, err := sox.Length(fnameIn)
	if err != nil {
		log.Errorf("could not get length of %s: %s", fnameIn, err.Error())
		return
	}
	pieceEnd, err := sox.Trim(fnameIn, seconds-0.5)
	if err != nil {
		log.Errorf("could not trim %s: %s", fnameIn, err.Error())
		return
	}
	pieceJoin, err := sox.Join(pieceEnd, fnameIn, pieceFront)
	defer func() {
		os.Remove(pieceFront)
		os.Remove(pieceEnd)
		os.Remove(pieceJoin)
	}()

	_, _, err = utils.Run(sox.GetBinary(), pieceJoin, "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", "norm", "gain", "-6")
	if err != nil {
		log.Error(err)
		return
	}
	_, _, err = utils.Run(sox.GetBinary(), "-t", "raw", "-c", fmt.Sprint(channels), "-r", fmt.Sprint(44100*oversampling), "--bits", "16", "--encoding", "signed-integer", "--endian", "little", "1.raw", fnameOut)
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func (f File) updateInfo(fnameIn string) (err error) {
	// determine the size
	finfo, err := os.Stat(fnameIn)
	if err != nil {
		log.Error(err)
		return
	}
	totalSamples := float64(finfo.Size()-44) / float64(f.Channels+1) / 2
	totalSamples = totalSamples - 22050*2*float64(f.Oversampling)
	fsize := totalSamples * float64(f.Channels+1) * 2 // total size excluding padding = totalSamples channels x 2 bytes
	sliceNum := len(f.SliceStart)
	if sliceNum == 0 {
		f.SliceStart = []float64{0.0}
		f.SliceStop = []float64{1.0}
		f.SliceType = []int{0}
		sliceNum = 1
	}
	slicesStart := []int32{}
	slicesEnd := []int32{}
	slicesType := []byte{}
	for i, _ := range f.SliceStart {
		slicesStart = append(slicesStart, int32(math.Round(f.SliceStart[i]*fsize))/4*4)
		slicesEnd = append(slicesEnd, int32(math.Round(f.SliceStop[i]*fsize))/4*4)
		slicesType = append(slicesType, byte(f.SliceType[i]))
	}

	BPMTempoMatch := uint8(0)
	if f.TempoMatch {
		BPMTempoMatch = 1
	}

	f.SpliceTrigger = 1
	if f.OneShot {
		f.SpliceTrigger = 0
	}

	sliceStartPtr := (*C.int)(unsafe.Pointer(&slicesStart[0]))
	sliceStopPtr := (*C.int)(unsafe.Pointer(&slicesEnd[0]))
	sliceTypePtr := (*C.schar)(unsafe.Pointer(&slicesType[0]))
	cStruct := C.SampleInfo_malloc(
		C.uint(fsize),
		C.uint(f.BPM),
		C.uchar(f.SplicePlayback),
		C.uchar(f.SpliceTrigger),
		C.uchar(BPMTempoMatch),
		C.uchar(f.Oversampling-1),
		C.uchar(f.Channels),
		C.uint(sliceNum),
		sliceStartPtr,
		sliceStopPtr,
		sliceTypePtr,
	)
	defer C.SampleInfo_free(cStruct)

	ret := C.SampleInfo_writeToDisk(cStruct)
	if ret != 0 {
		err = fmt.Errorf("Failed to write struct to file")
		return
	}

	cStruct2 := C.SampleInfo_readFromDisk()
	if cStruct2 == nil {
		fmt.Println("Failed to read struct from file")
		return
	}
	defer C.free(unsafe.Pointer(cStruct2))
	// fmt.Println("SampleInfo_getBPM", C.SampleInfo_getBPM(cStruct2))
	// fmt.Println("SampleInfo_getSliceNum", C.SampleInfo_getSliceNum(cStruct2))
	// for i := range slicesStart {
	// 	fmt.Println("SampleInfo_getSliceStart", i, C.SampleInfo_getSliceStart(cStruct2, C.ushort(i)))
	// }
	// for i := range slicesStart {
	// 	fmt.Println("SampleInfo_getSliceStart", i, C.SampleInfo_getSliceStart(cStruct2, C.ushort(i)))
	// }
	// for i := range slicesStart {
	// 	fmt.Println("SampleInfo_getSliceType", i, C.SampleInfo_getSliceType(cStruct2, C.ushort(i)))
	// }
	log.Infof("SampleInfo_getNumChannels: %d", C.SampleInfo_getNumChannels(cStruct2))

	err = os.Rename("sampleinfo.bin", fnameIn+".info")
	return
}
