package kickextract

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

const NUM_FREQUNCIES = 435
const KICK_FREQUNCIES = 204

func KickExtract(fname string, slice_start []float64, slice_end []float64) (kicks []bool, err error) {
	totalSamplesI, err := sox.NumSamples(fname)
	if err != nil {
		return
	}
	totalSamples := float64(totalSamplesI)
	kicks = make([]bool, len(slice_start))
	for i := 0; i < len(slice_start); i++ {
		// create temp file
		var f *os.File
		f, err = os.CreateTemp("", "sample")
		if err != nil {
			return
		}
		f.Close()
		fname2 := f.Name() + ".wav"
		defer os.Remove(fname2)
		_, _, err = run("sox", fname, fname2, "trim", fmt.Sprintf("%2.0fs", math.Round(totalSamples*slice_start[i])), fmt.Sprintf("%2.0fs", math.Round(totalSamples*(slice_end[i]-slice_start[i]))))
		if err != nil {
			return
		}
		var dataString string
		_, dataString, err = run("sox", fname2, "-n", "stat", "-freq")
		if err != nil {
			return
		}
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
		kickValue := 0.0
		for i := 0; i < KICK_FREQUNCIES; i++ {
			kickValue += dataMean[i]
		}
		fmt.Println(kickValue)
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
