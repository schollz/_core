package drumextract2

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"sync"

	"github.com/schollz/_core/core/src/onsetdetect"
	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
	"github.com/schollz/progressbar/v3"
)

var downloadedModel = false
var mutex sync.Mutex

const TRANSIENT_NUDGE = 22050 / 2

func DrumExtract2API(filePath string) (kickTransients []int, snareTransients []int, otherTransients []int, err error) {
	filename := utils.RandomString(10) + ".ogg"
	defer os.Remove(filename)

	cmd := exec.Command(sox.GetBinary(), filePath, "-r", "32000", "-c", "1", filename)
	err = cmd.Run()
	if err != nil {
		log.Error(err)
		return
	}

	file, err := os.Open(filename)
	if err != nil {
		return
	}
	defer file.Close()

	// Create a buffer to store the file contents
	var requestBody bytes.Buffer
	_, err = io.Copy(&requestBody, file)
	if err != nil {
		return
	}

	// Create the HTTP request
	req, err := http.NewRequest("PUT", "https://tool.getectocore.com/drumextract", &requestBody)
	if err != nil {
		log.Error(err)
		return
	}
	req.Header.Set("Content-Type", "application/octet-stream")

	// Send the HTTP request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		log.Error(err)
		return
	}
	defer resp.Body.Close()

	// Read and print the response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Error(err)
		return
	}

	var response struct {
		A []int `json:"a"`
		B []int `json:"b"`
		C []int `json:"c"`
	}
	err = json.Unmarshal(body, &response)
	if err != nil {
		log.Error(err)
		return
	}
	kickTransients = response.A
	snareTransients = response.B
	otherTransients = response.C
	return
}

func DrumExtract2(filePath string) (kickTransients []int, snareTransients []int, otherTransients []int, err error) {
	mutex.Lock()
	defer mutex.Unlock()
	kickTransients, snareTransients, _, _, err = drumExtract2(filePath)
	if err != nil {
		log.Error(err)
		return
	}

	numTransients := 8

	onsets, err := onsetdetect.OnsetDetect(filePath, numTransients)

	if err != nil {
		log.Error(err)
		return
	}
	otherTransients = make([]int, len(onsets))
	for i, v := range onsets {
		otherTransients[i] = int(math.Round(v * 44100))
	}
	otherTransients = filterDuplicates(otherTransients)
	return
}

func drumExtract2(filePath string) (kickTransients []int, snareTransients []int, otherTransients []int, onsets []int, err error) {
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

	// count total number
	count := 0
	count += len(kickTransients)
	count += len(snareTransients)
	count += len(otherTransients)

	// find all transients using onset detection
	allTransientsFloat, err := onsetdetect.OnsetDetect(filePath, count*2)
	onsets = make([]int, len(allTransientsFloat))
	for i, v := range allTransientsFloat {
		onsets[i] = int(v * 44100)
	}

	kickTransients = snapToOffsets(kickTransients, onsets)
	snareTransients = snapToOffsets(snareTransients, onsets)
	otherTransients = snapToOffsets(otherTransients, onsets)

	kickTransients = filterDuplicates(kickTransients)
	snareTransients = filterDuplicates(snareTransients)
	otherTransients = filterDuplicates(otherTransients)

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

	// if the first is 0 and the second is not, add a few samples to make it nonzero
	if kickTransients[0] == 0 && kickTransients[1] > 0 {
		kickTransients[0] = 10
	}
	if snareTransients[0] == 0 && snareTransients[1] > 0 {
		snareTransients[0] = 10
	}
	if otherTransients[0] == 0 && otherTransients[1] > 0 {
		otherTransients[0] = 10
	}

	return
}

const MIN_DIFFERENCE = 4410

// filterDuplicates removes duplicates from the slice that are less than MIN_DIFFERENCE apart.
func filterDuplicates(v []int) []int {
	if len(v) < 2 {
		return v
	}

	// First, sort the slice to make it easier to find duplicates.
	sort.Ints(v)

	// Create a new slice to hold the filtered values.
	result := make([]int, 0, len(v))
	result = append(result, v[0]) // Always include the first element.

	for i := 1; i < len(v); i++ {
		// Check if the current element is MIN_DIFFERENCE apart from the last element in the result.
		if v[i]-result[len(result)-1] >= MIN_DIFFERENCE {
			result = append(result, v[i])
		} else {
			log.Tracef("removing %d (%d)", i, v[i]-result[len(result)-1])
		}
	}

	return result
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

	if log.GetLevel() == "trace" {
		// debugging
		f, _ := os.Create(filePath + ".peaks")
		b, _ := json.Marshal(peaks)
		f.Write(b)
		f.Close()
		f, _ = os.Create(filePath + ".envelope")
		b, _ = json.Marshal(envelope)
		f.Write(b)
		f.Close()
		f, _ = os.Create(filePath + ".timeaxis")
		b, _ = json.Marshal(timeAxis)
		f.Write(b)
		f.Close()
	}

	return
}

func snapToOffsets(transients []int, onsets []int) []int {
	// for each transient, find whether there is an onset before it that is within 800 samples
	for i, v := range transients {
		// find the closest onset
		closestDiff := 100000000
		closestO := 0
		for _, o := range onsets {
			if o < v && (v-o) < int(closestDiff) {
				closestDiff = (v - o)
				closestO = o
			}
		}
		if closestDiff < TRANSIENT_NUDGE {
			log.Tracef("nudged %d to %d (%d)", transients[i], closestO, closestDiff)
			transients[i] = closestO
		}
	}
	return transients
}

func readWav(filePath string) ([]float64, int, error) {
	tmpRawFile := utils.RandomString(10) + ".raw"
	// convert the .wav file to raw data using the sox command
	cmdArray := []string{sox.GetBinary(), filePath, "-t", "raw", "-r", "44100", "-b", "16", "-c", "1", tmpRawFile}
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

	peakHeight := envelopeMean + math.Sqrt(envelopeStd)/4.0

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
