package main

import (
	"github.com/schollz/_core/core/src/zeptocore"
)

func main() {
	si := zeptocore.SampleInfo{
		Size:           100,
		Bpm:            350,
		PlayMode:       1,
		OneShot:        0,
		TempoMatch:     1,
		Oversampling:   1,
		NumChannels:    0,
		Version:        13,
		SpliceTrigger:  96,
		SpliceVariable: 1,
		SliceNum:       3,
		SliceStart:     []int32{123, 234, 456},
		SliceStop:      []int32{100, 200, 300},
		SliceType:      []int8{0, 1, 2},
	}
	sip := zeptocore.SampleInfoMarshal(&si)
	err := sip.WriteToFile("sampleinfo.bin")
	if err != nil {
		panic(err)
	}
}
