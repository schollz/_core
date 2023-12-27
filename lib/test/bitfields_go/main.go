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
	ret := C.writeStructToFile(4, 5)
	if ret != 0 {
		fmt.Println("Failed to write struct to file")
	} else {
		fmt.Println("Struct written to file successfully")
	}

	cStruct := C.readStructFromFile()
	if cStruct == nil {
		fmt.Println("Failed to read struct from file")
		return
	}
	defer C.free(unsafe.Pointer(cStruct))

	// Using accessor functions to get the values of a and b
	a := C.getA(cStruct)
	b := C.getB(cStruct)

	fmt.Printf("Read struct from file: a = %d, b = %d\n", a, b)
}
