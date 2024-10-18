package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"sort"
	"strings"

	log "github.com/schollz/logger"
	"github.com/youpy/go-wav"
)

var flagIn string
var flagOut string
var flagName string

func init() {
	flag.StringVar(&flagIn, "in", "folder", "folder of audio files")
	flag.StringVar(&flagName, "name", "audio", "name of audio data")
	flag.StringVar(&flagOut, "out", "audio.h", "converted audio data")
}

func main() {
	flag.Parse()
	log.SetLevel("trace")
	run()
}

func convertToUpper(s string) string {
	_, fileNameOnly := path.Split(s)
	codeName := strings.Map(func(r rune) rune {
		if r >= 'A' && r <= 'Z' {
			return r
		}
		if r >= '0' && r <= '9' {
			return r
		}
		return '_'
	}, strings.ToUpper(strings.Split(fileNameOnly, ".")[0]))
	return codeName
}
func run() (err error) {

	codeName := convertToUpper(flagName)
	codeNameLower := strings.ToLower(codeName)

	log.Tracef("%s %s", codeName, codeNameLower)
	// get all files in folder
	files, err := os.ReadDir(flagIn)
	if err != nil {
		log.Errorf("could not read folder: %s", err)
		return
	}

	// sort files
	sort.Slice(files, func(i, j int) bool {
		return files[i].Name() < files[j].Name()
	})

	for i, file := range files {
		tempFile := fmt.Sprintf("temp%d.wav", i)
		defer os.Remove(tempFile)
		log.Tracef("%s", strings.Join([]string{"sox", path.Join(flagIn, file.Name()), "-r", "44100", "-c", "1", "-b", "16", tempFile, "norm", "gain", "-6"}, " "))
		cmd := exec.Command("sox", path.Join(flagIn, file.Name()), "-r", "44100", "-c", "1", "-b", "16", tempFile, "norm", "gain", "-6")
		var output []byte
		output, err = cmd.CombinedOutput()
		if err != nil {
			log.Errorf("cmd failed: \n%s", output)
			return
		}
	}

	// start building the header
	var sb strings.Builder
	sb.WriteString("#include <pico/platform.h>\n\n")
	// create the fixed point function
	sb.WriteString(`inline int32_t q16_16_multiply_cue(int32_t a, int32_t b) {
  return (int32_t)((((int64_t)a) * b) >> 16);
}
`)
	sb.WriteString(fmt.Sprintf("#define %s_FILES %d\n", codeName, len(files)))
	// sb.WriteString(fmt.Sprintf("#define %s_SAMPLES %d\n", codeName, len(vals)))
	// sb.WriteString(fmt.Sprintf("const int16_t __in_flash() %s_audio[] = {\n", codeNameLower))
	for i := range files {
		sb.WriteString(fmt.Sprintf("#define %s_FILE_%s %d\n", codeName, convertToUpper(files[i].Name()), i))
	}

	// determine start/stop positions
	start := 0
	for i := range files {
		tempFile := fmt.Sprintf("temp%d.wav", i)
		var vals []int
		vals, err = convertWavToInts(tempFile)
		if err != nil {
			log.Errorf("could not convert wav to ints: %s", err)
			return
		}
		sb.WriteString(fmt.Sprintf("#define %s_%d_START %d\n", codeName, i, start))
		sb.WriteString(fmt.Sprintf("#define %s_%d_STOP %d\n", codeName, i, start+len(vals)))
		start += len(vals)
	}

	// write the audio data
	sb.WriteString(fmt.Sprintf("const int16_t __in_flash() %s_audio[] = {\n", codeNameLower))
	for i := range files {
		tempFile := fmt.Sprintf("temp%d.wav", i)
		var vals []int
		vals, err = convertWavToInts(tempFile)
		if err != nil {
			log.Errorf("could not convert wav to ints: %s", err)
			return
		}
		for _, v := range vals {
			sb.WriteString(fmt.Sprintf("%d, ", v))
		}
	}
	sb.WriteString("};\n")

	// write the logic to get the audio
	sb.WriteString(fmt.Sprintf("int8_t %s_do_play = -1;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("bool %s_is_playing = false;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("uint32_t %s_index = 0;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("uint8_t %s_file = 0;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("uint32_t %s_start_index = 0;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("uint32_t %s_stop_index = 0;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("uint8_t %s_volume = 100;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("void %s_audio_update(int32_t* samples, uint16_t num_samples, uint vol_main) {\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("  if (!%s_is_playing && %s_do_play<0) {\n", codeNameLower, codeNameLower))
	sb.WriteString(fmt.Sprintf("    return;\n"))
	sb.WriteString(fmt.Sprintf("  }\n"))
	sb.WriteString(fmt.Sprintf("  if (%s_do_play>=0 && !%s_is_playing) {\n", codeNameLower, codeNameLower))
	sb.WriteString(fmt.Sprintf("    %s_is_playing = true;\n", codeNameLower))
	// check which filename it is and set the start index using an if-else
	for i := range files {
		sb.WriteString(fmt.Sprintf("    if (%s_do_play %% %d == %d) {\n", codeNameLower, len(files), i))
		sb.WriteString(fmt.Sprintf("      %s_start_index = %s_%d_START;\n", codeNameLower, codeName, i))
		sb.WriteString(fmt.Sprintf("      %s_stop_index = %s_%d_STOP;\n", codeNameLower, codeName, i))
		sb.WriteString(fmt.Sprintf("    } else "))
	}
	sb.WriteString(fmt.Sprintf("{\n"))
	sb.WriteString(fmt.Sprintf("      %s_start_index = %s_%d_START;\n", codeNameLower, codeName, 0))
	sb.WriteString(fmt.Sprintf("      %s_stop_index = %s_%d_STOP;\n", codeNameLower, codeName, 0))
	sb.WriteString(fmt.Sprintf("    }\n"))
	sb.WriteString(fmt.Sprintf("    %s_index = %s_start_index;\n", codeNameLower, codeNameLower))
	sb.WriteString(fmt.Sprintf("    %s_do_play = -1;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("  }\n"))

	sb.WriteString(fmt.Sprintf("for (uint16_t i = 0; i < num_samples; i++) {\n"))
	sb.WriteString(fmt.Sprintf("  int32_t audio = (((int32_t)%s_audio[%s_index]) * %s_volume) >> 8;\n", codeNameLower, codeNameLower, codeNameLower))
	sb.WriteString(fmt.Sprintf("  audio = q16_16_multiply_cue((audio) << 16, vol_main);\n"))
	sb.WriteString(fmt.Sprintf("  samples[i * 2 + 0] += audio;\n"))
	sb.WriteString(fmt.Sprintf("  samples[i * 2 + 1] += audio;\n"))
	sb.WriteString(fmt.Sprintf("  %s_index++;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("  if (%s_index >= %s_stop_index) {\n", codeNameLower, codeNameLower))
	sb.WriteString(fmt.Sprintf("    %s_is_playing = false;\n", codeNameLower))
	sb.WriteString(fmt.Sprintf("    break;\n"))
	sb.WriteString(fmt.Sprintf("  }\n"))
	sb.WriteString(fmt.Sprintf("}\n"))

	sb.WriteString(fmt.Sprintf("  }\n"))

	// write sb to outFile
	outFile, err := os.Create(flagOut)
	if err != nil {
		log.Errorf("could not create file: %s", err)
		return
	}
	_, err = outFile.WriteString(sb.String())
	if err != nil {
		log.Errorf("could not write to file: %s", err)
	}
	outFile.Close()

	// do clang-format
	cmd := exec.Command("clang-format", "-i", flagOut)
	stdoutStderr, err := cmd.CombinedOutput()
	if err != nil {
		log.Errorf("cmd failed: \n%s", stdoutStderr)
	}

	return
}

func convertWavToInts(fname string) (vals []int, err error) {
	file, err := os.Open(fname)
	if err != nil {
		return
	}
	reader := wav.NewReader(file)
	n := 0
	vals = make([]int, 10000000)
	for {
		samples, err := reader.ReadSamples()

		for _, sample := range samples {
			v := reader.IntValue(sample, 0)
			vals[n] = v
			n++
		}

		if err == io.EOF {
			break
		}
	}
	err = file.Close()

	vals = vals[:n]
	return
}
