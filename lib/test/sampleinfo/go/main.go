package main

import (
	"encoding/binary"
	"fmt"
	"log"
	"os"
)

type SampleInfo struct {
	Size           uint32
	Bpm            uint16 // 0-511
	PlayMode       uint16 // 0-7
	OneShot        uint16 // 0-1 (off/on)
	TempoMatch     uint16 // 0-1 (off/on)
	Oversampling   uint16 // 0-1 (1x or 2x)
	NumChannels    uint16 // 0-1 (mono or stereo)
	SpliceTrigger  uint16
	SpliceVariable uint16 // 0-1 (off/on)
	SliceNum       uint8  // 0-255
	SliceCurrent   uint8  // 0-255
	SliceStart     []int32
	SliceStop      []int32
	SliceType      []int8
}

type SampleInfoPack struct {
	Size         uint32
	Flags        uint32 // Holds bpm, slice_num, slice_current, play_mode, one_shot, tempo_match, oversampling, and num_channels
	SpliceInfo   uint16 // Holds splice_trigger and splice_variable
	SliceNum     uint8  // 0-255
	SliceCurrent uint8  // 0-255
	SliceStart   []int32
	SliceStop    []int32
	SliceType    []int8
}

func SampleInfoMarshal(si *SampleInfo) *SampleInfoPack {
	pack := &SampleInfoPack{
		Size:         si.Size,
		Flags:        uint32(si.Bpm) | (uint32(si.PlayMode) << 9) | (uint32(si.OneShot) << 12) | (uint32(si.TempoMatch) << 13) | (uint32(si.Oversampling) << 14) | (uint32(si.NumChannels) << 15),
		SpliceInfo:   si.SpliceTrigger | (si.SpliceVariable << 15),
		SliceNum:     si.SliceNum,
		SliceCurrent: si.SliceCurrent,
		SliceStart:   make([]int32, len(si.SliceStart)),
		SliceStop:    make([]int32, len(si.SliceStop)),
		SliceType:    make([]int8, len(si.SliceType)),
	}
	copy(pack.SliceStart, si.SliceStart)
	copy(pack.SliceStop, si.SliceStop)
	copy(pack.SliceType, si.SliceType)
	return pack
}

func SampleInfoUnmarshal(pack *SampleInfoPack) *SampleInfo {
	si := &SampleInfo{
		Size:           pack.Size,
		Bpm:            uint16(pack.Flags & 0x1FF),
		PlayMode:       uint16((pack.Flags >> 9) & 0x7),
		OneShot:        uint16((pack.Flags >> 12) & 0x1),
		TempoMatch:     uint16((pack.Flags >> 13) & 0x1),
		Oversampling:   uint16((pack.Flags >> 14) & 0x1),
		NumChannels:    uint16((pack.Flags >> 15) & 0x1),
		SpliceTrigger:  pack.SpliceInfo & 0x7FFF,
		SpliceVariable: uint16((pack.SpliceInfo >> 15) & 0x1),
		SliceNum:       pack.SliceNum,
		SliceCurrent:   pack.SliceCurrent,
		SliceStart:     make([]int32, pack.SliceNum),
		SliceStop:      make([]int32, pack.SliceNum),
		SliceType:      make([]int8, pack.SliceNum),
	}
	copy(si.SliceStart, pack.SliceStart)
	copy(si.SliceStop, pack.SliceStop)
	copy(si.SliceType, pack.SliceType)
	return si
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
	err = binary.Write(file, binary.LittleEndian, sip.SliceCurrent)
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
	return nil
}

func SampleInfoPackReadFromFile(filename string) (*SampleInfoPack, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	pack := &SampleInfoPack{}

	log.Print("reading size")
	err = binary.Read(file, binary.LittleEndian, &pack.Size)
	if err != nil {
		return nil, err
	}
	log.Print("reading flags")
	err = binary.Read(file, binary.LittleEndian, &pack.Flags)
	if err != nil {
		return nil, err

	}
	log.Print("reading spliceinfo")
	err = binary.Read(file, binary.LittleEndian, &pack.SpliceInfo)
	if err != nil {
		return nil, err
	}
	err = binary.Read(file, binary.LittleEndian, &pack.SliceNum)
	if err != nil {
		return nil, err
	}
	err = binary.Read(file, binary.LittleEndian, &pack.SliceCurrent)
	if err != nil {
		return nil, err
	}

	pack.SliceStart = make([]int32, pack.SliceNum)
	pack.SliceStop = make([]int32, pack.SliceNum)
	pack.SliceType = make([]int8, pack.SliceNum)
	for i := range pack.SliceStart {
		err = binary.Read(file, binary.LittleEndian, &pack.SliceStart[i])
		if err != nil {
			return nil, err
		}
	}
	for i := range pack.SliceStop {
		err = binary.Read(file, binary.LittleEndian, &pack.SliceStop[i])
		if err != nil {
			return nil, err
		}
	}
	for i := range pack.SliceType {
		err = binary.Read(file, binary.LittleEndian, &pack.SliceType[i])
		if err != nil {
			return nil, err
		}
	}

	return pack, nil
}

func main() {
	si := SampleInfo{
		Size:           100,
		Bpm:            120,
		PlayMode:       1,
		OneShot:        1,
		TempoMatch:     1,
		Oversampling:   1,
		NumChannels:    1,
		SpliceTrigger:  96,
		SpliceVariable: 1,
		SliceNum:       3,
		SliceCurrent:   2,
		SliceStart:     []int32{123, 234, 456},
		SliceStop:      []int32{100, 200, 300},
		SliceType:      []int8{0, 1, 2},
	}
	sip := SampleInfoMarshal(&si)
	sip.WriteToFile("sampleinfo.bin")
	sip2, err := SampleInfoPackReadFromFile("sampleinfo.bin")
	if err != nil {
		panic(err)
	}
	fmt.Printf("sip: %+v\n", sip2)
	si2 := SampleInfoUnmarshal(sip2)
	fmt.Printf("si: %+v\n", si2)

}
