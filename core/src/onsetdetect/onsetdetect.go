package onsetdetect

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"net/http"
	"os"
	"os/exec"
	"runtime"
	"sort"
	"strconv"
	"strings"

	"github.com/muesli/clusters"
	"github.com/muesli/kmeans"
	"github.com/muesli/silhouette"
	"github.com/schollz/_core/core/src/sox"
	"github.com/schollz/_core/core/src/utils"
	log "github.com/schollz/logger"
)

func OnsetDetectAPI(fname string, numOnsets int) (onsets []float64, err error) {
	filename := utils.RandomString(10) + ".ogg"
	defer os.Remove(filename)

	cmd := exec.Command(sox.GetBinary(), fname, "-r", "44100", "-c", "1", filename)
	err = cmd.Run()
	if err != nil {
		log.Error(err)
		return
	}
	defer os.Remove(filename)

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
	req, err := http.NewRequest("PUT", fmt.Sprintf("http://tool.getectocore.com/onsetdetect?number=%d", numOnsets), &requestBody)
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

	err = json.Unmarshal(body, &onsets)
	return
}

func OnsetDetect(fname string, numOnsets int) (onsets []float64, err error) {
	allOnsets, err := getOnsets(fname, numOnsets)
	if err != nil {
		return
	}
	// find best onset for each window
	duration, err := sox.Length(fname)
	if err != nil {
		return
	}
	for i := 0; i < numOnsets; i++ {
		windowStart := (float64(i) - 0.5) / float64(numOnsets) * duration
		windowEnd := (float64(i+1) - 0.5) / float64(numOnsets) * duration
		onsetSubset := []float64{}
		for _, onset := range allOnsets {
			if onset >= windowStart && onset <= windowEnd {
				onsetSubset = append(onsetSubset, onset)
			}
		}
		if len(onsetSubset) == 0 {
			onsets = append(onsets, (float64(i))/float64(numOnsets)*duration)
		} else {
			onsets = append(onsets, findBiggestClusterCenter(onsetSubset))
		}
	}

	log.Debugf("onset detection: %+v", onsets)

	// onsets, err = findWindows(allOnsets, numOnsets)
	log.Trace(onsets)
	return
}

func findBiggestClusterCenter(data []float64) (center float64) {
	if len(data) < 3 {
		center = data[0]
		return
	}
	var d clusters.Observations
	for _, v := range data {
		d = append(d, clusters.Coordinates{
			v,
			0,
		})
	}

	km := kmeans.New()

	numClusters, _, err := silhouette.EstimateK(d, 8, km)
	if err != nil {
		if len(data) < 4 {
			numClusters = 2
		} else {
			numClusters = 3
		}
	}

	clusters, _ := km.Partition(d, numClusters)

	// find largest cluster
	maxCluster := 0
	maxClusterSize := 0
	for i, c := range clusters {
		if len(c.Observations) > maxClusterSize {
			maxClusterSize = len(c.Observations)
			maxCluster = i
		}
		log.Tracef("cluster %d) %.3f,%d\n", i+1, c.Center[0], len(c.Observations))
	}
	center = clusters[maxCluster].Center[0]
	log.Tracef("center: %.3f", center)
	return
}

func getOnsets(fname string, numOnsets int) (onsets []float64, err error) {
	type job struct {
		algo      string
		threshold float64
	}

	type result struct {
		result []float64
		err    error
	}

	duration, err := sox.Length(fname)
	if err != nil {
		return
	}

	joblist := []job{}

	// for _, algo := range []string{"energy", "hfc", "specflux", "madmom/OnsetDetector", "madmom/OnsetDetectorLL", "madmom/SuperFlux", "madmom/SuperFluxNN"} {
	for _, algo := range []string{"mkl", "complex", "energy", "hfc", "specflux"} {
		thresholds := []float64{3, 2, 1.5, 1, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.25, 0.2, 0.1, 0.05}
		if strings.Contains(algo, "madmom/") {
			thresholds = []float64{0.3, 0.2, 0.1}
		}
		for _, threshold := range thresholds {
			joblist = append(joblist, job{algo, threshold})
		}
	}

	numJobs := len(joblist)
	jobs := make(chan job, numJobs)
	results := make(chan result, numJobs)

	numCPU := runtime.NumCPU()
	runtime.GOMAXPROCS(numCPU)

	for i := 0; i < numCPU; i++ {
		go func(jobs <-chan job, results chan<- result) {
			for j := range jobs {
				var r result
				var out []byte
				var cmd *exec.Cmd
				if strings.Contains(j.algo, "madmom/") {
					// check if dev/madmom/.venv exists
					_, errExists := os.Stat("./dev/madmom/.venv/bin/python")
					if errExists != nil {
						r.err = fmt.Errorf("madmom not installed")
						results <- r
						return
					}
					cmd = exec.Command("./dev/madmom/.venv/bin/python", "./dev/"+j.algo, "-t", fmt.Sprint(j.threshold), "single", fname)
				} else {
					cmd = exec.Command("aubioonset", "-i", fname, "-B", "128", "-H", "128", "-t", fmt.Sprint(j.threshold), "-O", j.algo, "-M", fmt.Sprint(duration/128.0))
				}
				out, r.err = cmd.Output()
				for _, line := range strings.Split(string(out), "\n") {
					num, errNum := strconv.ParseFloat(line, 64)
					if errNum == nil {
						r.result = append(r.result, num)
					}
				}
				results <- r
			}
		}(jobs, results)
	}

	for _, j := range joblist {
		jobs <- j
	}
	close(jobs)

	data := [10000]float64{}
	j := 0
	for i := 0; i < numJobs; i++ {
		r := <-results
		if r.err != nil {
			err = r.err
		} else {
			if (j == 0 && len(r.result) > 4) || (len(r.result) < 2*numOnsets && len(r.result) > numOnsets/2) {
				for _, v := range r.result {
					if j < len(data) {
						data[j] = v
						j++
					}
				}
			}
		}
	}
	onsets = data[:j]
	sort.Float64s(onsets)

	return
}

func findWindows(data []float64, numOnsets int) (top16 []float64, err error) {
	if len(data) == 0 {
		err = fmt.Errorf("no onsets found")
		return
	}
	min, max := minMax(data)
	min = 0
	win := 0.0125
	type Window struct {
		min, max float64
		data     []float64
		avg      float64
	}
	windowMap := make(map[float64]Window)
	for i := min; i < max-win; i += win / 2 {
		w := Window{i, i + win, getRange(data, i, i+win), max}
		if len(w.data) > 0 {
			w.avg = average(w.data)
		}
		windowMap[toFixed(w.avg, 2)] = w
	}

	windows := make([]Window, len(windowMap))
	j := 0
	for _, v := range windowMap {
		windows[j] = v
		j++
	}

	sort.Slice(windows, func(i, j int) bool {
		return len(windows[i].data) > len(windows[j].data)
	})

	top16 = make([]float64, numOnsets)
	for i, w := range windows {
		if i == numOnsets {
			break
		}
		top16[i] = w.avg
	}
	if len(top16) == 0 {
		err = fmt.Errorf("no onsets found")
	}

	sort.Float64s(top16)

	// make sure to get the first one
	if top16[0] > 0.15 {
		for i := 2; i < numOnsets; i++ {
			top16[i] = windows[i].avg
		}
		sort.Slice(windows, func(i, j int) bool {
			return windows[i].avg < windows[j].avg
		})
		top16[0] = windows[0].avg
		sort.Float64s(top16)
	}

	for i, v := range top16 {
		top16[i] = toFixed(v, 3)
	}

	return
}

func average(arr []float64) (result float64) {
	if len(arr) == 0 {
		return 0.0
	}
	sum := 0.0
	for _, v := range arr {
		sum += v
	}
	return sum / float64(len(arr))
}

func getRange(arr []float64, min, max float64) (rng []float64) {
	data := make([]float64, len(arr))
	j := 0
	for _, v := range arr {
		if v >= min && v <= max {
			data[j] = v
			j++
		}
		// assume arr is sorted
		if v > max {
			break
		}
	}
	if j > 0 {
		rng = data[:j]
	}
	return
}

func minMax(array []float64) (float64, float64) {
	var max float64 = array[0]
	var min float64 = array[0]
	for _, value := range array {
		if max < value {
			max = value
		}
		if min > value {
			min = value
		}
	}
	return min, max
}

func round(num float64) int {
	return int(num + math.Copysign(0.5, num))
}

func toFixed(num float64, precision int) float64 {
	output := math.Pow(10, float64(precision))
	return float64(round(num*output)) / output
}
