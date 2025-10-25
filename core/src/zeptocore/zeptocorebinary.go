package zeptocore

import (
	"encoding/binary"
	"math"
	"os"
	"strings"

	log "github.com/schollz/logger"
)

type SampleInfo struct {
	Size           uint32
	Bpm            uint16 // 0-511
	PlayMode       uint16 // 0-7
	OneShot        uint16 // 0-1 (off/on)
	TempoMatch     uint16 // 0-1 (off/on)
	Oversampling   uint16 // 0-1 (1x or 2x)
	NumChannels    uint16 // 0-1 (mono or stereo)
	Version        uint16 // 0-127
	SpliceTrigger  uint16
	SpliceVariable uint16 // 0-1 (off/on)
	SliceNum       uint8  // 0-255
	SliceStart     []int32
	SliceStop      []int32
	SliceType      []int8
	Transients     [][]int32
}

type SampleInfoPack struct {
	Size       uint32
	Flags      uint32 // Holds bpm, slice_num, slice_current, play_mode, one_shot, tempo_match, oversampling, and num_channels
	SpliceInfo uint16 // Holds splice_trigger and splice_variable
	SliceNum   uint8  // 0-255
	SliceStart []int32
	SliceStop  []int32
	SliceType  []int8
	Transients [][]int32
}

func SampleInfoMarshal(si *SampleInfo) *SampleInfoPack {
	// constrain variables
	if si.Bpm > 510 {
		si.Bpm = 510
	}
	if si.PlayMode > 7 {
		si.PlayMode = 7
	}
	if si.OneShot > 1 {
		si.OneShot = 1
	}
	if si.TempoMatch > 1 {
		si.TempoMatch = 1
	}
	if si.Oversampling > 1 {
		si.Oversampling = 1
	}
	if si.NumChannels > 1 {
		si.NumChannels = 1
	}
	if si.SpliceTrigger > 32767 {
		si.SpliceTrigger = 32767
	}
	if si.SpliceVariable > 1 {
		si.SpliceVariable = 1
	}
	if si.SliceNum > 255 {
		si.SliceNum = 255
	}
	pack := &SampleInfoPack{
		Size:       si.Size,
		Flags:      uint32(si.Bpm) | (uint32(si.PlayMode) << 9) | (uint32(si.OneShot) << 12) | (uint32(si.TempoMatch) << 13) | (uint32(si.Oversampling) << 14) | (uint32(si.NumChannels) << 15) | (uint32(si.Version) << 16),
		SpliceInfo: si.SpliceTrigger | (si.SpliceVariable << 15),
		SliceNum:   si.SliceNum,
		SliceStart: make([]int32, len(si.SliceStart)),
		SliceStop:  make([]int32, len(si.SliceStop)),
		SliceType:  make([]int8, len(si.SliceType)),
	}

	copy(pack.SliceStart, si.SliceStart)
	copy(pack.SliceStop, si.SliceStop)
	copy(pack.SliceType, si.SliceType)
	for i := range si.Transients {
		pack.Transients = append(pack.Transients, make([]int32, len(si.Transients[i])))
		copy(pack.Transients[i], si.Transients[i])
	}
	return pack
}

func (sip *SampleInfoPack) WriteToFile(filename string) error {
	file, err := os.OpenFile(filename, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		return err
	}
	defer file.Close()

	err = binary.Write(file, binary.LittleEndian, sip.Size)
	if err != nil {
		return err
	}

	err = binary.Write(file, binary.LittleEndian, sip.Flags)
	if err != nil {
		return err
	}
	err = binary.Write(file, binary.LittleEndian, sip.SpliceInfo)
	if err != nil {
		return err
	}
	err = binary.Write(file, binary.LittleEndian, sip.SliceNum)
	if err != nil {
		return err
	}
	for _, v := range sip.SliceStart {
		err = binary.Write(file, binary.LittleEndian, v)
		if err != nil {
			return err
		}
	}
	for _, v := range sip.SliceStop {
		err = binary.Write(file, binary.LittleEndian, v)
		if err != nil {
			return err
		}
	}
	for _, v := range sip.SliceType {
		err = binary.Write(file, binary.LittleEndian, v)
		if err != nil {
			return err
		}
	}
	log.Debugf("writing transients: %d", sip.Transients)
	for _, transients := range sip.Transients {
		count := 0
		for _, v := range transients {
			if v == 0 {
				continue
			}
			count++
		}
		// write nums
		err = binary.Write(file, binary.LittleEndian, uint16(count))
		if err != nil {
			return err
		}
	}
	for _, transients := range sip.Transients {
		for _, v := range transients {
			if v == 0 {
				continue
			}
			// scale v to uint16
			// it only allows first 24 seconds to be logged but its okay
			v = v / 16
			if v > 65535 {
				v = 0
			}
			err = binary.Write(file, binary.LittleEndian, uint16(v))
			if err != nil {
				return err
			}
		}
	}

	return nil
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
	BPMTempoMatch := uint8(0)
	if f.TempoMatch {
		BPMTempoMatch = 1
	}
	oneshot := 0
	if f.OneShot {
		oneshot = 1
	}
	splicevariable := 0
	if f.SpliceVariable {
		splicevariable = 1
	}

	sampleinfo := &SampleInfo{
		Size:         uint32(fsize),
		Bpm:          uint16(f.BPM),
		PlayMode:     uint16(f.SplicePlayback),
		OneShot:      uint16(oneshot),
		TempoMatch:   uint16(BPMTempoMatch),
		Oversampling: uint16(f.Oversampling - 1),
		NumChannels:  uint16(f.Channels),
		// Versoin 1+ will load transients
		Version:        1,
		SpliceTrigger:  uint16(f.SpliceTrigger),
		SpliceVariable: uint16(splicevariable),
		SliceNum:       uint8(sliceNum),
		SliceStart:     []int32{},
		SliceStop:      []int32{},
		SliceType:      []int8{},
		Transients:     [][]int32{},
	}
	for i := range f.SliceStart {
		sampleinfo.SliceStart = append(sampleinfo.SliceStart, int32(math.Round(f.SliceStart[i]*fsize))/4*4)
		sampleinfo.SliceStop = append(sampleinfo.SliceStop, int32(math.Round(f.SliceStop[i]*fsize))/4*4)
		sampleinfo.SliceType = append(sampleinfo.SliceType, int8(f.SliceType[i]))
	}
	for i := range f.Transients {
		transients := []int32{}
		for j := range f.Transients[i] {
			transients = append(transients, int32(f.Transients[i][j]))
		}
		sampleinfo.Transients = append(sampleinfo.Transients, transients)
	}
	if strings.HasSuffix(fnameIn, "0.wav") {
		log.Debugf("writing sample info %s to %+v", fnameIn, sampleinfo)
	}

	sampleinfoPack := SampleInfoMarshal(sampleinfo)
	err = sampleinfoPack.WriteToFile(fnameIn + ".info")
	return
}
