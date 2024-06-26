package drumextract2

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"

	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
	"github.com/schollz/progressbar/v3"
)

var downloadedModel = false
var mutex sync.Mutex

func DrumExtract2(filePath string) (kickTransients []int, snareTransients []int, otherTransients []int, err error) {
	mutex.Lock()
	defer mutex.Unlock()
	return drumExtract2(filePath)
}

func drumExtract2(filePath string) (kickTransients []int, snareTransients []int, otherTransients []int, err error) {
	kickTransients = make([]int, 16)
	snareTransients = make([]int, 16)
	if !downloadedModel {
		log.Trace("model not downloaded, skipping")
		return
	}
	// check if file exists
	_, filename := filepath.Split(filePath)
	filenameWithoutExtension := strings.TrimSuffix(filename, filepath.Ext(filename))
	stemFile := FOLDER_MODEL + "_output/modelo_final/" + filenameWithoutExtension + "/bombo.wav"
	if _, err = os.Stat(stemFile); err != nil {
		demucsCmd := []string{"demucs", "--repo", FOLDER_MODEL, "-o", FOLDER_MODEL + "_output", "-n", "modelo_final", filePath}
		log.Trace(strings.Join(demucsCmd, " "))
		cmd := exec.Command(demucsCmd[0], demucsCmd[1:]...)
		err = cmd.Run()
		if err != nil {
			fmt.Println("Error running demucs:", err)
		}
	}
	kickTransients, err = getTransientSamplePositions(FOLDER_MODEL + "_output/modelo_final/" + filenameWithoutExtension + "/bombo.wav")
	if err != nil {
		log.Error(err)
	}
	snareTransients, err = getTransientSamplePositions(FOLDER_MODEL + "_output/modelo_final/" + filenameWithoutExtension + "/redoblante.wav")
	if err != nil {
		log.Error(err)
	}
	otherTransients, err = getTransientSamplePositions(FOLDER_MODEL + "_output/modelo_final/" + filenameWithoutExtension + "/platillos.wav")
	if err != nil {
		log.Error(err)
	}

	// truncate arrays if they are greater than 16
	if len(kickTransients) > 16 {
		kickTransients = kickTransients[:16]
	}
	if len(snareTransients) > 16 {
		snareTransients = snareTransients[:16]
	}
	if len(otherTransients) > 16 {
		otherTransients = otherTransients[:16]
	}

	// pad with zeros if they are smaller than 16
	for len(kickTransients) < 16 {
		kickTransients = append(kickTransients, 0)
	}
	for len(snareTransients) < 16 {
		snareTransients = append(snareTransients, 0)
	}
	for len(otherTransients) < 16 {
		otherTransients = append(otherTransients, 0)
	}

	return
}

func getTransientSamplePositions(filePath string) (transients []int, err error) {
	audioData, frameRate, err := readWav(filePath)
	if err != nil {
		fmt.Println("Error reading WAV file:", err)
		return
	}

	hopSize := 64
	windowSize := 128
	envelope := envelopeFollower(audioData, frameRate, windowSize, hopSize)

	timeAxis := make([]float64, len(envelope))
	for i := range timeAxis {
		timeAxis[i] = float64(i*hopSize) / float64(frameRate)
	}

	peaks := getPeaks(envelope, timeAxis)
	transients = make([]int, len(peaks))
	for i, v := range peaks {
		transients[i] = int(math.Round(v * float64(frameRate)))
	}
	return
}

func readWav(filePath string) ([]float64, int, error) {
	tmpRawFile := utils.RandomString(10) + ".raw"
	// convert the .wav file to raw data using the sox command
	cmdArray := []string{"sox", filePath, "-t", "raw", "-r", "44100", "-b", "16", "-c", "1", tmpRawFile}
	log.Trace(strings.Join(cmdArray, " "))
	cmd := exec.Command(cmdArray[0], cmdArray[1:]...)
	defer os.Remove(tmpRawFile)
	var out bytes.Buffer
	cmd.Stdout = &out
	err := cmd.Run()
	if err != nil {
		return nil, 0, err
	}

	file, err := os.Open(tmpRawFile)
	if err != nil {
		return nil, 0, err
	}
	defer file.Close()

	// Get file size
	fileInfo, err := file.Stat()
	if err != nil {
		return nil, 0, err
	}
	fileSize := fileInfo.Size()

	// Check if file size is a multiple of 2 (since we're reading 16-bit values)
	if fileSize%2 != 0 {
		return nil, 0, fmt.Errorf("file size is not a multiple of 2 bytes")
	}

	// Read the file content into a byte slicej
	byteValues := make([]byte, fileSize)
	_, err = file.Read(byteValues)
	if err != nil {
		return nil, 0, err
	}

	// Convert the byte slice to a slice of uint16
	numValues := int(fileSize / 2)
	values := make([]uint16, numValues)
	for i := 0; i < numValues; i++ {
		values[i] = binary.LittleEndian.Uint16(byteValues[i*2 : i*2+2])
	}

	// Convert the uint16 values to float64
	audioData := make([]float64, numValues)
	for i, v := range values {
		audioData[i] = float64(int16(v)) / math.MaxUint16
	}

	return audioData, 44100, nil
}

func envelopeFollower(audioData []float64, frameRate, windowSize, hopSize int) []float64 {
	envelope := []float64{}
	for i := 0; i < len(audioData)-windowSize; i += hopSize {
		window := audioData[i : i+windowSize]
		maxVal := 0.0
		for _, v := range window {
			absVal := math.Abs(v)
			if absVal > maxVal {
				maxVal = absVal
			}
		}
		envelope = append(envelope, maxVal)
	}
	return envelope
}

func getPeaks(envelope []float64, timeAxis []float64) []float64 {
	envelopeMean := 0.0
	for _, v := range envelope {
		envelopeMean += v
	}
	envelopeMean /= float64(len(envelope))
	envelopeStd := 0.0
	for _, v := range envelope {
		envelopeStd += (v - envelopeMean) * (v - envelopeMean)
	}
	envelopeStd = math.Sqrt(envelopeStd / float64(len(envelope)))

	peakHeight := envelopeMean + math.Sqrt(envelopeStd)/2.0

	peaks := []float64{}
	inPeak := false
	lastPeak := -1.0
	peakMinimumDistance := 0.1

	for i, v := range envelope {
		if v > peakHeight && !inPeak && timeAxis[i]-lastPeak > peakMinimumDistance {
			peaks = append(peaks, timeAxis[i])
			inPeak = true
		} else if v < peakHeight && inPeak {
			inPeak = false
			lastPeak = timeAxis[i]
		} else if v > peakHeight && !inPeak && timeAxis[i]-lastPeak <= peakMinimumDistance {
			lastPeak = timeAxis[i]
		}
	}
	return peaks
}

func max(data []float64) float64 {
	maxVal := data[0]
	for _, v := range data {
		if v > maxVal {
			maxVal = v
		}
	}
	return maxVal
}

const FOLDER_MODEL = "drum_separation_model"

func DownloadModel() (err error) {

	// check if folder exists
	if _, err = os.Stat(FOLDER_MODEL); os.IsNotExist(err) {

		// check if zip file exists
		if _, err = os.Stat(FOLDER_MODEL + ".zip"); os.IsNotExist(err) {
			log.Info("Downloading model")
			req, _ := http.NewRequest("GET", "https://schollz.com/"+FOLDER_MODEL+".zip", nil)
			resp, _ := http.DefaultClient.Do(req)
			defer resp.Body.Close()

			f, _ := os.OpenFile(FOLDER_MODEL+".zip", os.O_CREATE|os.O_WRONLY, 0644)
			defer f.Close()

			bar := progressbar.DefaultBytes(
				resp.ContentLength,
				"downloading",
			)
			io.Copy(io.MultiWriter(f, bar), resp.Body)
		}

		err = utils.Unzip(FOLDER_MODEL+".zip", ".")
	}
	downloadedModel = err == nil
	return
}
