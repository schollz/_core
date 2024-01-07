package kickextract

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"os/exec"
	"runtime"
	"strconv"
	"strings"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

const NUM_FREQUNCIES = 435
const KICK_FREQUNCIES = 204

// KickExtract takes the original filename and the fractional slice start/stop points
// and prints out a slice of booleans indicating whether or not there is a kick at each slice
func KickExtract(fname string, slice_start []float64, slice_end []float64) (kicks []bool, err error) {
	totalSamplesI, err := sox.NumSamples(fname)
	if err != nil {
		return
	}
	totalSamples := float64(totalSamplesI)
	var numJobs = len(slice_start)
	kicks = make([]bool, numJobs)
	type job struct {
		index int
	}
	type result struct {
		index     int
		kickvalue float64
		err       error
	}

	jobs := make(chan job, numJobs)
	results := make(chan result, numJobs)

	runtime.GOMAXPROCS(runtime.NumCPU())

	for w := 1; w <= runtime.NumCPU(); w++ {
		go func(jobs <-chan job, results chan<- result) {
			for j0 := range jobs {
				func(j job) {
					var r result
					r.index = j.index
					defer func() {
						results <- r
					}()

					// create temp file
					var f *os.File
					f, r.err = os.CreateTemp("", "sample")
					if r.err != nil {
						return
					}
					f.Close()
					os.Remove(f.Name())
					fname2 := f.Name() + ".wav"
					defer os.Remove(fname2)

					// split into a splice
					_, _, r.err = run("sox",
						fname, fname2,
						"trim",
						fmt.Sprintf("%2.0fs", math.Round(totalSamples*slice_start[j.index])),
						fmt.Sprintf("%2.0fs", math.Round(totalSamples*(slice_end[j.index]-slice_start[j.index]))))
					if r.err != nil {
						return
					}

					// gather frequency information
					var dataString string
					_, dataString, r.err = run("sox", fname2, "-n", "stat", "-freq")
					if r.err != nil {
						return
					}

					// collate frequencies to get averages
					datas := make(map[int][]float64)
					for i := 0; i < 435; i++ {
						datas[i] = []float64{}
					}
					for _, line := range strings.Split(dataString, "\n") {
						fields := strings.Fields(line)
						if len(fields) != 2 {
							continue
						}
						freq, _ := strconv.ParseFloat(fields[0], 64)
						freq = math.Round(math.Log10(freq) * 100)
						freqI := int(freq)
						if freqI < 0 || freqI > NUM_FREQUNCIES {
							continue
						}
						power, _ := strconv.ParseFloat(fields[1], 64)
						datas[freqI] = append(datas[freqI], power)
					}

					// calculate the average in each bin
					dataMean := make([]float64, NUM_FREQUNCIES)
					for i := 0; i < NUM_FREQUNCIES; i++ {
						dataMean[i] = 0
						if len(datas[i]) > 0 {
							sum := 0.0
							num := 0.0
							for _, v := range datas[i] {
								sum += v
								num++
							}
							dataMean[i] = sum / num
						}
					}

					// calculate the kick value
					for i := 0; i < KICK_FREQUNCIES; i++ {
						r.kickvalue += dataMean[i]
					}
				}(j0)
			}
		}(jobs, results)
	}

	for j := 0; j < numJobs; j++ {
		jobs <- job{j}
	}
	close(jobs)

	kickvalues := make([]float64, numJobs)
	for a := 0; a < numJobs; a++ {
		r := <-results
		kickvalues[r.index] = r.kickvalue
		if r.err != nil {
			err = r.err
			log.Errorf("error in job %d: %s", r.index, err.Error())
		}
	}

	// calculate the average kick value
	kickvalueMean := 0.0
	for _, v := range kickvalues {
		kickvalueMean += v
	}
	kickvalueMean /= float64(numJobs)

	// calculate the standard deviation
	kickvalueStd := 0.0
	for _, v := range kickvalues {
		kickvalueStd += math.Pow(v-kickvalueMean, 2)
	}
	kickvalueStd /= float64(numJobs)
	kickvalueStd = math.Sqrt(kickvalueStd)

	// calculate the kicks
	for i := 0; i < numJobs; i++ {
		if kickvalues[i] > kickvalueMean+kickvalueStd {
			kicks[i] = true
		}
	}
	return
}

func run(args ...string) (string, string, error) {
	log.Trace(strings.Join(args, " "))
	baseCmd := args[0]
	cmdArgs := args[1:]
	cmd := exec.Command(baseCmd, cmdArgs...)
	var outb, errb bytes.Buffer
	cmd.Stdout = &outb
	cmd.Stderr = &errb
	err := cmd.Run()
	if err != nil {
		log.Errorf("%s -> '%s'", strings.Join(args, " "), err.Error())
		log.Error(outb.String())
		log.Error(errb.String())
		err = fmt.Errorf("%s%s", outb.String(), errb.String())
	}
	return outb.String(), errb.String(), err
}
