package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"os"
)

func main() {
	buf := new(bytes.Buffer)
	var data = []any{
		uint32(61374), // a
		uint32(54),    // b
		uint8(254),    // c
		int8(-12),     // d
	}
	data = append(data, int8(7))
	data = append(data, []uint32{32, 33, 34, 35, 36, 37, 38})
	s := []byte("hello, world!")
	data = append(data, int16(len(s)))
	data = append(data, s)
	for _, v := range data {
		err := binary.Write(buf, binary.LittleEndian, v)
		if err != nil {
			fmt.Println("binary.Write failed:", err)
		}
	}
	fmt.Printf("%d bytes: %x", len(buf.Bytes()), buf.Bytes())
	bufSize := new(bytes.Buffer)
	binary.Write(bufSize, binary.LittleEndian, uint32(len(buf.Bytes())))

	f, _ := os.Create("test")
	f.Write(bufSize.Bytes())
	f.Write(buf.Bytes())
	f.Close()
}
