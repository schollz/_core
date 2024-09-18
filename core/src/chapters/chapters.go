package chapters

import (
	"fmt"
	"os"

	"github.com/go-audio/wav"
	log "github.com/schollz/logger"
)

var DebugMode = false

func GetSliceMarkers(filePath string) (start []float64, end []float64, err error) {
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
			timeInSeconds := float64(cue.SampleOffset) / float64(sampleRate)
			cuePoints = append(cuePoints, timeInSeconds)
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
		}
	}

	// Check for sample loops in the metadata
	if decoder.Metadata.SamplerInfo != nil && len(decoder.Metadata.SamplerInfo.Loops) > 0 {
		log.Tracef("\nSample Loops in %s:\n", filePath)
		for i, loop := range decoder.Metadata.SamplerInfo.Loops {
			// Convert start and end sample offsets to seconds
			startTime := float64(loop.Start) / float64(sampleRate)
			endTime := float64(loop.End) / float64(sampleRate)
			log.Tracef("Loop %d: Start Time: %.3f seconds, End Time: %.3f seconds\n", i+1, startTime, endTime)
			start = append(start, startTime)
			end = append(end, endTime)
		}
	}
	// normalize to 3 decimal places
	for i := range start {
		start[i] = float64(int(start[i]*1000)) / 1000
		end[i] = float64(int(end[i]*1000)) / 1000
	}
	return
}
