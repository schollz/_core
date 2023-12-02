package zeptocore

import (
	"encoding/json"
	"fmt"
	"os"

	"github.com/go-audio/wav"
	log "github.com/schollz/logger"
)

// define error for no metadata present
var ErrNoMetadata = fmt.Errorf("no metadata present")

func SetMetadata(fname string, metadata File) (err error) {
	in, err := os.Open(fname)
	if err != nil {
		log.Error(err)
		return
	}
	d := wav.NewDecoder(in)
	buf, err := d.FullPCMBuffer()
	if err != nil {
		log.Error(err)
		return
	}
	in.Close()

	out, err := os.Create(fname)
	if err != nil {
		return
	}
	defer out.Close()

	e := wav.NewEncoder(out,
		buf.Format.SampleRate,
		int(d.BitDepth),
		buf.Format.NumChannels,
		int(d.WavAudioFormat))
	if err = e.Write(buf); err != nil {
		log.Error(err)
		return
	}
	e.Metadata = &wav.Metadata{}
	b, err := json.Marshal(metadata)
	if err != nil {
		log.Error(err)
		return
	}
	e.Metadata.Comments = string(b)
	err = e.Close()
	if err != nil {
		log.Error(err)
		return
	}
	return
}

func GetMetadata(fname string) (metadata File, err error) {
	f, err := os.Open(fname)
	if err != nil {
		panic(err)
	}
	defer f.Close()
	dec := wav.NewDecoder(f)
	dec.ReadMetadata()
	if err := dec.Err(); err != nil {
		log.Error(err)
	}
	if dec.Metadata == nil {
		err = ErrNoMetadata
		return
	}
	err = json.Unmarshal([]byte(dec.Metadata.Comments), &metadata)
	if err != nil {
		return
	}
	if metadata.SliceStart == nil {
		err = ErrNoMetadata
		return
	}
	return
}
