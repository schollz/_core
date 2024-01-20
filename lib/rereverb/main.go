package main

import (
	"os"

	log "github.com/schollz/logger"
	"github.com/schollz/sox"
	"github.com/schollz/superfx"
)

func main() {
	log.SetLevel("trace")
	fname2, err := rereverb("amen_bpm165.wav")
	if err != nil {
		log.Error(err)
	}
	err = os.Rename(fname2, "test.wav")
	if err != nil {
		log.Error(err)
	}

}

func rereverb(fname string) (fname2 string, err error) {
	totalBeats, bpm, err := sox.GetBPM(fname)
	if err != nil {
		return
	}
	log.Debugf("fname: %s, beats: %2.1f, bpm: %2.1f", fname, totalBeats, bpm)
	fname2, err = sox.Gain(fname, -3)
	if err != nil {
		return
	}
	pieces := []string{}
	for beat := float64(0); beat < totalBeats; beat++ {
		var piece string
		piece, err = sox.Trim(fname2, beat*60/bpm, 60/bpm)
		if err != nil {
			log.Error(err)
			break
		}
		piece, err = sox.Reverse(piece)
		if err != nil {
			log.Error(err)
			break
		}
		piece, err = sox.SilenceAppend(piece, 60/bpm)
		if err != nil {
			log.Error(err)
			break
		}
		piece = superfx.SCPath(piece)
		piece, err = superfx.Effect(piece, "reverberate")
		if err != nil {
			log.Error(err)
		}
		// fname2, err = superfx.Effect(fname2, "lpf_rampup", 2, 0)
		// if err != nil {
		// 	log.Error(err)
		// }
		piece, err = sox.Reverse(piece)
		if err != nil {
			return
		}
		piece, err = sox.Left(piece)
		if err != nil {
			return
		}

		pieces = append(pieces, piece)
	}
	fname2, err = sox.Join(pieces...)
	if err != nil {
		log.Error(err)
		return
	}
	fname2, err = sox.ResampleRate(fname2, 44100, 16)
	if err != nil {
		log.Error(err)
		return
	}

	err = superfx.Stop()
	if err != nil {
		log.Error(err)
	}
	return
}
