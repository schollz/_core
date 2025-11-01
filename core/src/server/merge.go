package server

import (
	"os"

	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/zeptocore"
	log "github.com/schollz/logger"
)

// Merges the files together and writes metadata
// to preserve the slice start times as boundaries between each file
func Merge(fnames []string, fnameout string) (err error) {
	var metadata zeptocore.File

	filesToMerge := make([]string, len(fnames))
	totalTime := 0.0
	allOneshot := true // Track if all files are in oneshot mode
	for i, fname := range fnames {
		var f zeptocore.File
		f, err = zeptocore.Get(fname)
		if err != nil {
			log.Error(err)
			return
		}
		// Check if this file is NOT oneshot
		if !f.OneShot {
			allOneshot = false
		}
		for _, v := range f.SliceStart {
			metadata.SliceStart = append(metadata.SliceStart, totalTime+v*f.Duration)
		}
		for _, v := range f.SliceStop {
			metadata.SliceStop = append(metadata.SliceStop, totalTime+v*f.Duration)
		}
		// append silence
		silenceDuration := 0.1
		filesToMerge[i], err = sox.SilenceAppend(f.PathToAudio, silenceDuration)
		if err != nil {
			log.Error(err)
			return
		}
		defer os.Remove(filesToMerge[i])
		totalTime += (f.Duration + silenceDuration)
	}

	// If all files are oneshot, mark the merged file as oneshot too
	if allOneshot {
		metadata.OneShot = true
		metadata.TempoMatch = false
		metadata.SplicePlayback = 1
		log.Debugf("all files are oneshot, merged file will be oneshot with tempomatch disabled")
	}

	for i, v := range metadata.SliceStart {
		metadata.SliceStart[i] = v / totalTime
	}
	for i, v := range metadata.SliceStop {
		metadata.SliceStop[i] = v / totalTime
	}

	fname2, err := sox.Join(filesToMerge...)
	if err != nil {
		log.Error(err)
		return
	}

	log.Tracef("%s -> %s", fname2, fnameout)
	err = os.Rename(fname2, fnameout)
	if err != nil {
		log.Error(err)
		return
	}
	log.Trace("check1")
	log.Trace(sox.Length(fnameout))

	log.Tracef("setting metadata for %s", fnameout)
	log.Tracef("metadata: %+v", metadata.SliceStart)
	log.Tracef("metadata: %+v", metadata.SliceStop)
	err = zeptocore.SetMetadata(fnameout, zeptocore.Metadata{
		SliceStart: metadata.SliceStart,
		SliceStop:  metadata.SliceStop,
	})
	if err != nil {
		log.Error(err)
	}
	log.Trace("check2")
	log.Trace(sox.Length(fnameout))

	// Create a .wav copy for wavesurfer to load in the webview
	fnameoutWav := fnameout[:len(fnameout)-4] + ".wav"
	err = sox.Convert(fnameout, fnameoutWav)
	if err != nil {
		log.Error(err)
		return
	}

	// If all merged files were oneshot, set the merged file properties
	// We save directly without triggering Regenerate() to avoid slow processing
	if allOneshot {
		var mergedFile zeptocore.File
		mergedFile, err = zeptocore.Get(fnameout)
		if err != nil {
			log.Error(err)
			return
		}
		// Set properties directly and save without regeneration
		mergedFile.OneShot = true
		mergedFile.TempoMatch = false
		mergedFile.SplicePlayback = 1
		err = mergedFile.SaveNoDebounce()
		if err != nil {
			log.Error(err)
			return
		}
		log.Debugf("saved merged file with oneshot properties (no regeneration triggered)")
	}

	return
}
