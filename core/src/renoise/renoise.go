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
	// catch panic
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("panic: %v", r)
			log.Error(err)
		}
	}()

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
	log.Tracef("data: %+v", data)
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
	for i := range markers {
		if i == 0 {
			continue
		}
		start = append(start, float64(markers[i-1])/float64(numSamples))
		end = append(end, float64(markers[i])/float64(numSamples))
		if i == len(markers)-1 {
			start = append(start, float64(markers[i])/float64(numSamples))
			end = append(end, 1)
		}
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
		log.Error(err)
		return
	}
	// find the first instance of <Samples> and remove everything before it
	start := 0
	for i, b := range xmlBytes {
		if b == '<' && string(xmlBytes[i:i+8]) == "<Samples" {
			start = i
			break
		}
	}
	if start == 0 {
		err = fmt.Errorf("could not find <Samples>")
		log.Error(err)
		return
	}
	xmlBytes = xmlBytes[start:]
	// find the first instance of </Samples> and remove everything after it
	end := 0
	for i, b := range xmlBytes {
		if b == '<' && string(xmlBytes[i:i+10]) == "</Samples>" {
			end = i + 10
			break
		}
	}
	if end == 0 {
		err = fmt.Errorf("could not find </Samples>")
		log.Error(err)
		return
	}

	xmlBytes = xmlBytes[:end]
	// prepend xml header
	xmlBytes = append([]byte("<?xml version=\"1.0\" encoding=\"UTF-8\"?><RenoiseInstrument>\n"), xmlBytes...)
	// append xml footer
	xmlBytes = append(xmlBytes, []byte("\n</RenoiseInstrument>")...)

	os.WriteFile("test.xml", xmlBytes, 0644)
	err = xml.Unmarshal(xmlBytes, &data)
	if err != nil {
		log.Error(err)
	}

	log.Tracef("data: %+v", data)
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
