package chapters

import (
	"fmt"
	"os"

	"github.com/go-audio/wav"
	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

var DebugMode = false

func GetSliceMarkers(filePath string) (start []float64, end []float64, err error) {
	// catch panic
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("panic: %v", r)
			log.Error(err)
		}
	}()

	// Open the audio file
	file, err := os.Open(filePath)
	if err != nil {
		log.Errorf("Error opening file: %v\n", err)
		return
	}
	defer file.Close()

	// Create a WAV decoder
	decoder := wav.NewDecoder(file)

	// Check if it's a valid WAV file
	if !decoder.IsValidFile() {
		log.Errorf("Invalid WAV file: %s\n", filePath)
		return
	}

	// Get the sample rate from the decoder
	sampleRate := decoder.SampleRate
	if sampleRate == 0 {
		log.Errorf("Sample rate not found in WAV file: %s\n", filePath)
		return
	}

	// Read metadata
	decoder.ReadMetadata()

	// Check if there are cue points in the metadata
	if decoder.Metadata == nil || (len(decoder.Metadata.CuePoints) == 0 && decoder.Metadata.SamplerInfo == nil) {
		fmt.Println("No chapters (cue points) or sample loops found in the WAV file metadata.")
		return
	}

	// Print cue points as chapters in seconds
	if len(decoder.Metadata.CuePoints) > 0 {
		cuePoints := []float64{}
		for _, cue := range decoder.Metadata.CuePoints {
			// Convert sample offset to seconds
			cuePoints = append(cuePoints, float64(cue.SampleOffset))
		}
		if len(cuePoints) > 0 {
			for i, v := range cuePoints {
				if i == 0 {
					if v > 0 {
						start = append(start, 0)
						end = append(end, v)
					}
				} else {
					start = append(start, cuePoints[i-1])
					end = append(end, v)
				}
			}
			// set to end of file
		}
	}

	// Check for sample loops in the metadata
	if decoder.Metadata.SamplerInfo != nil && len(decoder.Metadata.SamplerInfo.Loops) > 0 {
		log.Tracef("\nSample Loops in %s:\n", filePath)
		for i, loop := range decoder.Metadata.SamplerInfo.Loops {
			// Convert start and end sample offsets to seconds
			startTime := float64(loop.Start)
			endTime := float64(loop.End)
			log.Tracef("Loop %d: Start Time: %.3f seconds, End Time: %.3f seconds\n", i+1, startTime, endTime)
			start = append(start, startTime)
			end = append(end, endTime)
		}
	}

	numSamples, err := sox.NumSamples(filePath)
	if err != nil {
		log.Error(err)
		return
	}
	// normalize to 1.0
	for i := range start {
		start[i] = start[i] / float64(numSamples)
		end[i] = end[i] / float64(numSamples)
	}
	if end[len(end)-1] < 1 {
		start = append(start, end[len(end)-1])
		end = append(end, 1.0)
	}
	return
}
