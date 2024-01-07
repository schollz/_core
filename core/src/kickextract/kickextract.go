package kickextract

import (
	"bytes"
	"fmt"
	"math"
	"os"
	"os/exec"
	"strings"

	"github.com/schollz/_core/core/src/sox"
	log "github.com/schollz/logger"
)

func KickExtract(fname string, slice_start []float64, slice_end []float64) (kicks []bool, err error) {
	totalSamplesI, err := sox.NumSamples(fname)
	if err != nil {
		return
	}
	totalSamples := float64(totalSamplesI)
	kicks = make([]bool, len(slice_start))
	fnames := make([]string, len(slice_start))
	for i := 0; i < len(slice_start); i++ {
		// create temp file
		var f *os.File
		f, err = os.CreateTemp("", "sample")
		if err != nil {
			return
		}
		f.Close()
		fnames[i] = f.Name() + ".wav"
		run("sox", fname, fnames[i], "trim", fmt.Sprintf("%2.0fs", math.Round(totalSamples*slice_start[i])), fmt.Sprintf("%2.0fs", math.Round(totalSamples*(slice_end[i]-slice_start[i]))))
		os.Remove(fnames[i])
	}
	for i := 0; i < len(slice_start); i++ {
		fmt.Println(fnames[i])
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
