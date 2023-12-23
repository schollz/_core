package renoise

import (
	"archive/zip"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"path/filepath"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

var DebugMode = false

func GetSliceMarkers(fname string) (filename string, start []float64, end []float64, err error) {
	files, err := unzipFile(fname)
	if err != nil {
		log.Error(err)
		return
	}

	data, err := parseXML("Instrument.xml")
	if err != nil {
		log.Error(err)
		return
	}
	markers := []int{}
	for _, d := range data.Samples.Sample[0].SliceMarkers.SliceMarker {
		markers = append(markers, d.SamplePosition)
	}
	if len(markers) < 2 {
		err = fmt.Errorf("not enough markers")
		log.Error(err)
		return
	}
	for _, f := range files {
		if f != "Instrument.xml" {
			filename = f
			break
		}
	}
	if filename == "" {
		err = fmt.Errorf("could not find sample data")
		log.Error(err)
		return
	}
	numSamples, err := sox.NumSamples(filename)
	if err != nil {
		log.Error(err)
		return
	}
	log.Tracef("filename: %s, numSamples: %d", filename, numSamples)
	for i, _ := range markers {
		if i == 0 {
			continue
		}
		start = append(start, float64(markers[i-1])/float64(numSamples))
		end = append(end, float64(markers[i])/float64(numSamples))
	}

	if !DebugMode {
		os.Remove("Instrument.xml")
	}

	return
}

// RenoiseInstrument was generated 2023-10-28 12:18:29 by https://xml-to-go.github.io/ in Ukraine.
type RenoiseInstrument struct {
	Samples struct {
		Text   string `xml:",chardata"`
		Sample []struct {
			Name         string `xml:"Name"`
			FileName     string `xml:"FileName"`
			LoopMode     string `xml:"LoopMode"`
			LoopRelease  string `xml:"LoopRelease"`
			LoopStart    int    `xml:"LoopStart"`
			LoopEnd      int    `xml:"LoopEnd"`
			SliceMarkers struct {
				Text        string `xml:",chardata"`
				SliceMarker []struct {
					SamplePosition int `xml:"SamplePosition"`
				} `xml:"SliceMarker"`
			} `xml:"SliceMarkers"`
			SingleSliceTriggerEnabled string `xml:"SingleSliceTriggerEnabled"`
			IsAlias                   string `xml:"IsAlias"`
		} `xml:"Sample"`
	} `xml:"Samples"`
}

func parseXML(xmlFile string) (data RenoiseInstrument, err error) {
	// Read the XML file into a byte slice.
	xmlBytes, err := os.ReadFile(xmlFile)
	if err != nil {
		return
	}
	err = xml.Unmarshal(xmlBytes, &data)

	return
}

func unzipFile(zipFile string) ([]string, error) {
	// Open the zip file.
	reader, err := zip.OpenReader(zipFile)
	if err != nil {
		return nil, err
	}
	defer reader.Close()

	// Create a list to store the unzipped files.
	unzippedFiles := []string{}

	// Iterate over the files in the zip file and unzip them.
	for _, file := range reader.File {
		// Create a new file to store the unzipped content.
		_, fname := filepath.Split(file.Name)
		unzippedFile, err := os.Create(fname)
		if err != nil {
			return nil, err
		}

		// Write the unzipped content to the file.
		b, _ := file.Open()
		_, err = io.Copy(unzippedFile, b)
		if err != nil {
			return nil, err
		}

		// Close the unzipped file.
		err = unzippedFile.Close()
		if err != nil {
			return nil, err
		}

		// Add the unzipped file to the list.
		unzippedFiles = append(unzippedFiles, fname)
	}

	return unzippedFiles, nil
}
