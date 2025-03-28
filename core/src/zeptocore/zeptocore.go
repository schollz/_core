package zeptocore

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/bep/debounce"
	"github.com/dhowden/tag"
	"github.com/schollz/_core/core/src/chapters"
	"github.com/schollz/_core/core/src/drumextract"
	"github.com/schollz/_core/core/src/drumextract2"
	"github.com/schollz/_core/core/src/onsetdetect"
	"github.com/schollz/_core/core/src/op1"
	"github.com/schollz/_core/core/src/renoise"
	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
)

type File struct {
	Filename       string
	Duration       float64
	PathToFile     string
	PathToAudio    string
	BPM            int
	SliceStart     []float64 // fractional (0-1)
	SliceStop      []float64 // fractional (0-1)
	SliceType      []int     // 0 = normal, 1 = kick
	Transients     [][]int   // kick, snare, other
	TempoMatch     bool
	OneShot        bool
	Channels       int  // 0 if mono, 1 if stereo
	Oversampling   int  // 1, 2, or 4
	SpliceTrigger  int  // 0, 16, 32, 48, 64, 80, 96, 112, 128
	SpliceVariable bool // 0,1 (off/on)
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

func Get(pathToOriginal string, dropaudiofilemode ...string) (f File, err error) {
	_, filename := filepath.Split(pathToOriginal)
	f = File{
		Filename:   filename,
		PathToFile: pathToOriginal,
	}
	if f.Load() == nil {
		log.Debugf("loaded %s from disk", pathToOriginal)
		f.debounceSave = debounce.New(321 * time.Millisecond)
		f.debounceRegen = debounce.New(321 * time.Millisecond)
		return
	}
	log.Debugf("creating new %s, could not find cache", pathToOriginal)
	// create new file
	f = File{
		Filename:       filename,
		PathToFile:     pathToOriginal,
		PathToAudio:    pathToOriginal,
		debounceSave:   debounce.New(321 * time.Millisecond),
		debounceRegen:  debounce.New(321 * time.Millisecond),
		OneShot:        false,
		TempoMatch:     true,
		Channels:       0,
		Oversampling:   1,
		SpliceVariable: false,
		SpliceTrigger:  96,
	}
	// make transients 3 x 16
	f.Transients = make([][]int, 3)
	for i := range f.Transients {
		f.Transients[i] = make([]int, 16)
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
	} else if filepath.Ext(f.PathToFile) == ".wav" {
		// try to collect start/stop information from chapters
		log.Tracef("attempting chapters %s", f.PathToFile)
		f.SliceStart, f.SliceStop, errSliceDetect = chapters.GetSliceMarkers(f.PathToFile)
		if errSliceDetect == nil && len(f.SliceStart) > 0 && len(f.SliceStop) == len(f.SliceStart) {
			log.Trace("detected slices from .wav chapters")
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
	} else if filepath.Ext(f.Filename) == ".ogg" {
		// try to collect start/stop information from artist/album
		fopen, err := os.Open(f.PathToAudio)
		if err != nil {
			log.Error(err)
		}
		metadata, err := tag.ReadFrom(fopen) // Read metadata from the file
		if err != nil {
			log.Error(err)
		}
		if strings.Contains(metadata.Comment(), "oneshot") {
			f.OneShot = true
			f.SplicePlayback = 1
			f.TempoMatch = false
			f.BPM = 120
		}
		if strings.Contains(metadata.Comment(), "singleslice") {
			f.SliceStart = []float64{0.0}
			f.SliceStop = []float64{1.0}
		} else {
			spliceStartBytes := []byte(metadata.Artist())
			spliceStopBytes := []byte(metadata.Album())
			duration, _ := sox.Length(f.PathToAudio)
			log.Debugf("spliceStartBytes: %s", spliceStartBytes)
			log.Debugf("spliceStopBytes: %s", spliceStopBytes)
			if len(spliceStartBytes) > 0 && len(spliceStopBytes) > 0 {
				err = json.Unmarshal(spliceStartBytes, &f.SliceStart)
				if err != nil {
					log.Error(err)
				}
				err = json.Unmarshal(spliceStopBytes, &f.SliceStop)
				if err != nil {
					log.Error(err)
				}
				for i := range f.SliceStart {
					f.SliceStart[i] = f.SliceStart[i] / duration
				}
				for i := range f.SliceStop {
					f.SliceStop[i] = f.SliceStop[i] / duration
				}
			}
		}

		fopen.Close()
		if len(f.SliceStart) > 0 {
			errSliceDetect = nil
		}

	}

	if len(dropaudiofilemode) > 0 && dropaudiofilemode[0] == "oneshot" {
		log.Debugf("setting %s to oneshot", f.PathToFile)
		f.OneShot = true
		f.SplicePlayback = 1
		f.TempoMatch = false
		f.BPM = 120
		f.SliceStart = []float64{0.0}
		f.SliceStop = []float64{1.0}
		errSliceDetect = nil
	}

	// get the number of channels
	_, channels, _, _ := sox.Info(f.PathToAudio)
	channels = channels - 1
	if channels < 0 {
		channels = 0
	} else if channels > 1 {
		channels = 1
	}
	f.Channels = channels

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
	log.Infof("beats: %f", beats)
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

	slicesPerBeat := float64(len(f.SliceStart)) / beats
	f.SpliceTrigger = int(math.Round(2*96/slicesPerBeat*4) / 4)
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

	go func() {
		_, errDemucs := exec.LookPath("demucs")
		if errDemucs == nil {
			// get transients
			transients1, transients2, transients3, errTransients := drumextract2.DrumExtract2(f.PathToAudio)
			if errTransients == nil {
				// reload in case its been too long
				f.Load()
				f.Transients[0] = transients1
				f.Transients[1] = transients2
				f.Transients[2] = transients3
				log.Debugf("saving transients for %s", f.PathToAudio)
				f.Save()
				f.Regenerate()
			}
		} else {
			// use the public API
			transients1, transients2, transients3, errTransients := drumextract2.DrumExtract2API(f.PathToAudio)
			if errTransients == nil {
				// reload in case its been too long
				f.Load()
				f.Transients[0] = transients1
				f.Transients[1] = transients2
				f.Transients[2] = transients3
				log.Debugf("saving transients from API for %s", f.PathToAudio)
				f.Save()
				f.Regenerate()
			}
		}

	}()

	return
}

func (f File) SaveNoDebounce() (err error) {
	log.Tracef("writing %s.json", f.PathToFile)
	fi, err := os.Create(fmt.Sprintf("%s.json", f.PathToFile))
	if err != nil {
		log.Error(err)
		return
	}
	defer fi.Close()

	log.Debugf("saving %s\n\n%+v\n\n", f.PathToFile, f)

	err = json.NewEncoder(fi).Encode(f)
	if err != nil {
		log.Error(err)
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

		log.Debugf("saving %s\n\n%+v\n\n", f.PathToFile, f)

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
		folder, filename := filepath.Split(f.PathToAudio)
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

		// create the variatoins

		// // create tape emulations
		// _, err = exec.LookPath("lv2file")
		// if err == nil {
		// 	// create worker group
		// 	emulations := []string{"TC-260", "808 Comp and Tone", "That Dirty LoFi", "Old Telephone"}
		// 	var wg sync.WaitGroup
		// 	wg.Add(len(emulations))
		// 	for i, emulation := range emulations {
		// 		go func(i int, emulation string) {
		// 			defer wg.Done()
		// 			log.Tracef("emulation: %s on file %d", emulation, i)
		// 			// convert fname0 to stereo
		// 			fname0_stereo, err := sox.Stereo(fname0)
		// 			if err != nil {
		// 				log.Error(err)
		// 				return
		// 			}
		// 			defer os.Remove(fname0_stereo)
		// 			fnameEmulation := path.Join(folder, fmt.Sprintf("%s.0.%d.wav", filenameWithouExt, i))
		// 			cmdString := []string{"lv2file", "-i", fname0_stereo, "-o", fnameEmulation, "-P", emulation, "https://github.com/jatinchowdhury18/AnalogTapeModel"}
		// 			cmd := exec.Command(cmdString[0], cmdString[1:]...)
		// 			stdout, errRun := cmd.CombinedOutput()
		// 			if errRun != nil {
		// 				log.Errorf("cmd: %+v", cmdString)
		// 				log.Errorf("stdout: %s", stdout)
		// 				log.Error(errRun)
		// 				return
		// 			}
		// 			// convert fname0 to stereo
		// 			fname1_stereo, err := sox.Stereo(fname1)
		// 			if err != nil {
		// 				log.Error(err)
		// 				return
		// 			}
		// 			defer os.Remove(fname1_stereo)
		// 			fnameEmulation = path.Join(folder, fmt.Sprintf("%s.1.%d.wav", filenameWithouExt, i))
		// 			cmd = exec.Command("lv2file", "-i", fname1_stereo, "-o", fnameEmulation, "-P", emulation, "https://github.com/jatinchowdhury18/AnalogTapeModel")
		// 			stdout, errRun = cmd.CombinedOutput()
		// 			if errRun != nil {
		// 				log.Errorf("cmd: %+v", cmdString)
		// 				log.Errorf("stdout: %s", stdout)
		// 				log.Error(errRun)
		// 				return
		// 			}
		// 		}(i, emulation)
		// 	}
		// 	wg.Wait()
		// }

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
	mu.Lock()
	defer mu.Unlock()
	different := f.BPM != bpm
	f.BPM = bpm
	f.SaveNoDebounce()
	go func() {
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetSplicePlayback(playback int) {
	mu.Lock()
	defer mu.Unlock()
	different := f.SplicePlayback != playback
	f.SplicePlayback = playback
	log.Debugf("SetSplicePlayback %d", playback)
	f.SaveNoDebounce()
	go func() {
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) UpdateSliceTypes() {
	// calculate the splice type
	kicks, snares, err := drumextract.DrumExtract(f.PathToAudio, f.SliceStart, f.SliceStop)
	if err != nil {
		log.Error(err)
	}
	f.SliceType = make([]int, len(kicks))
	for i := range kicks {
		if kicks[i] && snares[i] {
			f.SliceType[i] = 3
		} else if kicks[i] {
			f.SliceType[i] = 1
		} else if snares[i] {
			f.SliceType[i] = 2
		} else {
			f.SliceType[i] = 0
		}
	}
	log.Tracef("slice types: %+v", f.SliceType)
}

func (f *File) SetTransient(i int, j int, value int) {
	mu.Lock()
	defer mu.Unlock()
	log.Debugf("transient before: %d", f.Transients[i][j])
	f.Transients[i][j] = value
	log.Debugf("transient after: %d", f.Transients[i][j])

	f.SaveNoDebounce()
	go func() {
		f.Regenerate()
	}()
}

func (f *File) SetSlices(sliceStart []float64, sliceEnd []float64) (sliceType []int) {
	mu.Lock()
	defer mu.Unlock()
	f.SliceStart = sliceStart
	f.SliceStop = sliceEnd
	f.UpdateSliceTypes()
	f.SaveNoDebounce()
	go func() {
		f.Regenerate()
	}()
	return f.SliceType
}

func (f *File) SetOversampling(oversampling int) {
	mu.Lock()
	defer mu.Unlock()
	different := f.Oversampling != oversampling
	f.Oversampling = oversampling
	f.SaveNoDebounce()
	go func() {
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

func (f *File) SetSpliceTrigger(spliceTrigger int) {
	mu.Lock()
	defer mu.Unlock()
	different := f.SpliceTrigger != spliceTrigger
	f.SpliceTrigger = spliceTrigger
	f.SaveNoDebounce()
	go func() {
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetSpliceVariable(spliceVariable bool) {
	mu.Lock()
	defer mu.Unlock()
	log.Tracef("setting splice variable to %v", spliceVariable)
	different := f.SpliceVariable != spliceVariable
	f.SpliceVariable = spliceVariable
	f.SaveNoDebounce()
	go func() {
		if different {
			f.Regenerate()
		}
	}()
}

func (f *File) SetOneshot(oneshot bool) {
	mu.Lock()
	defer mu.Unlock()
	f.OneShot = oneshot
	log.Debugf("SetOneshot %v", oneshot)
	f.SaveNoDebounce()
	go func() {
		f.Regenerate()
	}()
}

func (f *File) SetTempoMatch(TempoMatch bool) {
	mu.Lock()
	defer mu.Unlock()
	different := f.TempoMatch != TempoMatch
	f.TempoMatch = TempoMatch
	f.SaveNoDebounce()
	go func() {
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
