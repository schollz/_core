package main

/*
#cgo CFLAGS: -I.
#include "writeStructToFile.h"
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func main() {
	sliceStart := []int32{10, 20, 30}
	sliceStartPtr := (*C.int)(unsafe.Pointer(&sliceStart[0]))
	sliceStop := []int32{100, 200, 300}
	sliceStopPtr := (*C.int)(unsafe.Pointer(&sliceStop[0]))
	bpm := 120
	beats := 8
	play_mode := 0
	splice_trigger := 3
	tempo_match := 1
	oversampling := 1
	num_channels := 0
	cStruct := C.SampleInfo_malloc(C.uint(bpm), C.uint(beats), C.uchar(play_mode), C.uchar(splice_trigger), C.uchar(tempo_match), C.uchar(oversampling), C.uchar(num_channels),
		C.uint(len(sliceStart)), sliceStartPtr, sliceStopPtr)
	fmt.Println("SampleInfo_getBPM", C.SampleInfo_getBPM(cStruct))
	fmt.Println("SampleInfo_getSliceNum", C.SampleInfo_getSliceNum(cStruct))
	for i := range sliceStart {
		fmt.Println("SampleInfo_getSliceStart", i, C.SampleInfo_getSliceStart(cStruct, C.ushort(i)))
	}
	defer C.SampleInfo_free(cStruct)

	ret := C.SampleInfo_writeToDisk(cStruct)
	if ret != 0 {
		fmt.Println("Failed to write struct to file")
		return
	}

	cStruct2 := C.SampleInfo_readFromDisk()
	if cStruct2 == nil {
		fmt.Println("Failed to read struct from file")
		return
	}
	defer C.free(unsafe.Pointer(cStruct2))
	fmt.Println("SampleInfo_getBPM", C.SampleInfo_getBPM(cStruct2))
	fmt.Println("SampleInfo_getSliceNum", C.SampleInfo_getSliceNum(cStruct2))
	for i := range sliceStart {
		fmt.Println("SampleInfo_getSliceStart", i, C.SampleInfo_getSliceStart(cStruct2, C.ushort(i)))
	}
	for i := range sliceStart {
		fmt.Println("SampleInfo_getSliceStop", i, C.SampleInfo_getSliceStop(cStruct2, C.ushort(i)))
	}

}
