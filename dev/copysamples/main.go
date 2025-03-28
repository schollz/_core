package main

import (
	_ "embed"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"syscall"
	"time"

	log "github.com/schollz/logger"
	"github.com/schollz/progressbar/v3"
)

//go:embed pop.wav
var popWav []byte

var argMountingName string
var argSourceFolder string
var argUserName string
var argLogger string

func init() {
	flag.StringVar(&argMountingName, "name", "ZEPTOCORE", "the name of the drive to mount as")
	flag.StringVar(&argSourceFolder, "src", "../starting_samples2", "the folder to copy to the drive")
	flag.StringVar(&argUserName, "user", "zns", "the user name to give permissions to the drive")
	flag.StringVar(&argLogger, "log", "info", "the log level")
}

func lsblk() (lines []string, err error) {
	cmd := exec.Command("lsblk", "-P")
	out, err := cmd.Output()
	if err != nil {
		log.Error(err)
		return
	}
	lines = strings.Split(string(out), "\n")
	return
}

type Filesystem struct {
	Name  string
	Size  float64
	Mount string
}

func watchFilesystem(systemAdd chan Filesystem) {
	originalLines, err := lsblk()
	if err != nil {
		return
	}
	for {
		var newLines []string
		newLines, err = lsblk()
		if err != nil {
			return
		}
		// check if a new line is added
		for i := range newLines {
			hasLine := false
			for j := range originalLines {
				if newLines[i] == originalLines[j] {
					hasLine = true
					break
				}
			}
			if !hasLine {
				if strings.Contains(newLines[i], `TYPE="part"`) {
					// get name from `NAME="sda1"` using regex
					nameRegex := regexp.MustCompile(`NAME="(.+?)"`)
					name := nameRegex.FindStringSubmatch(newLines[i])[1]
					sizeRegex := regexp.MustCompile(`SIZE="(.+?)"`)
					sizeString := sizeRegex.FindStringSubmatch(newLines[i])[1]
					// remove non-numeric characters from sizeString
					sizeString = strings.ReplaceAll(sizeString, "M", "")
					sizeString = strings.ReplaceAll(sizeString, "K", "")
					sizeString = strings.ReplaceAll(sizeString, "G", "")
					size, _ := strconv.ParseFloat(sizeString, 64)
					mountRegex := regexp.MustCompile(`MOUNTPOINTS="(.+?)"`)
					submatches := mountRegex.FindStringSubmatch(newLines[i])
					if len(submatches) > 1 {
						mount := submatches[1]
						systemAdd <- Filesystem{Name: name, Size: size, Mount: mount}
					} else {
						log.Tracef(newLines[i])
					}
				}
				break
			}
		}
		originalLines = newLines
		time.Sleep(100 * time.Millisecond)
	}
}

// CopyFile copies a file from src to dst. If src and dst files exist, and are
// the same, then return success. Otherise, attempt to create a hard link
// between the two files. If that fail, copy the file contents from src to dst.
func CopyFile(src, dst string, pb *progressbar.ProgressBar) (err error) {
	sfi, err := os.Stat(src)
	if err != nil {
		return
	}
	if !sfi.Mode().IsRegular() {
		// cannot copy non-regular files (e.g., directories,
		// symlinks, devices, etc.)
		return fmt.Errorf("CopyFile: non-regular source file %s (%q)", sfi.Name(), sfi.Mode().String())
	}
	dfi, err := os.Stat(dst)
	if err != nil {
		if !os.IsNotExist(err) {
			return
		}
	} else {
		if !(dfi.Mode().IsRegular()) {
			return fmt.Errorf("CopyFile: non-regular destination file %s (%q)", dfi.Name(), dfi.Mode().String())
		}
		if os.SameFile(sfi, dfi) {
			return
		}
	}
	if err = os.Link(src, dst); err == nil {
		return
	}
	err = copyFileContents(src, dst, pb)
	return
}

// copyFileContents copies the contents of the file named src to the file named
// by dst. The file will be created if it does not already exist. If the
// destination file exists, all it's contents will be replaced by the contents
// of the source file.
func copyFileContents(src, dst string, pb *progressbar.ProgressBar) (err error) {
	in, err := os.Open(src)
	if err != nil {
		return
	}
	defer in.Close()
	out, err := os.Create(dst)
	if err != nil {
		return
	}
	defer func() {
		cerr := out.Close()
		if err == nil {
			err = cerr
		}
	}()
	if _, err = io.Copy(io.MultiWriter(out, pb), in); err != nil {
		return
	}
	err = out.Sync()
	return
}

func reformatFilesystem(partition Filesystem) (err error) {
	// first check the size
	log.Tracef("size: %f", partition.Size)
	if partition.Size>100 {
		err = fmt.Errorf("size is not correct")
		return
	}

	destinationFolder := fmt.Sprintf("/media/%s/%s", argUserName, argMountingName)
	// unmount the drive
	// sudo umount /dev/sdd1
	fmt.Printf("%s: unmounting...", destinationFolder)
	cmd := exec.Command("sudo", "umount", "/dev/"+partition.Name)
	out, err := cmd.CombinedOutput()
	if err != nil {
		log.Errorf("could not unmount: %s", out)
		return
	}

	// format the drive
	fmt.Print("formatting...")
	cmd = exec.Command("sudo", "mkfs.vfat", "-n", argMountingName, "/dev/"+partition.Name)
	out, err = cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
		return
	}
	log.Debugf("formatting: %s", out)

	// create mount folder
	fmt.Print("mounting...")
	cmd = exec.Command("sudo", "mkdir", "-p", destinationFolder)
	out, err = cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
	}
	log.Debugf("mkdir: %s", out)

	// give mount folder user permissions
	cmd = exec.Command("sudo", "chown", "-R", "zns:zns", destinationFolder)
	out, err = cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
	}
	log.Debugf("permissions: %s", out)

	// mount the drive
	cmd = exec.Command("sudo", "mount", "/dev/"+partition.Name, destinationFolder)
	out, err = cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
	}
	log.Debugf("mount: %s", out)

	// collect the files
	// copy every file in folder to mounted drive
	fmt.Println("copying...")
	totalBytes := int64(0)
	// get a list of all files in the source folder by walking through it, recursively
	sourceFiles := []string{}
	destFiles := []string{}
	err = filepath.Walk(argSourceFolder,
		func(p string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if info.IsDir() {
				return nil
			}
			sourceFiles = append(sourceFiles, p)
			destFiles = append(destFiles, path.Join(destinationFolder, strings.TrimPrefix(p, argSourceFolder)))
			totalBytes += info.Size()
			return nil
		})
	if err != nil {
		log.Error(err)
		return
	}

	pb := progressbar.NewOptions64(totalBytes,
		progressbar.OptionClearOnFinish(),
		progressbar.OptionShowBytes(true),
	)
	// copy the files
	for i := range sourceFiles {
		folderName, _ := path.Split(destFiles[i])
		// check if the folder exists
		if _, err = os.Stat(folderName); os.IsNotExist(err) {
			// create the folder
			err = os.MkdirAll(folderName, 0755)
			if err != nil {
				log.Error(err)
				return
			}
		}
		// copy the file
		err = CopyFile(sourceFiles[i], destFiles[i], pb)
		// cmd = exec.Command("cp", sourceFiles[i], destFiles[i])
		// out, err = cmd.CombinedOutput()
		if err != nil {
			log.Error(err)
		}
		log.Tracef("copy %s->%s %s", sourceFiles[i], destFiles[i], out)
	}

	time.Sleep(1 * time.Second)

	// umount the drive
	fmt.Print("\n....unmounting...")
	cmd = exec.Command("sudo", "umount", "/dev/"+partition.Name)
	out, err = cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
	}

	fmt.Println("done.")
	go func() {
		playBeep()
	}()
	return
}

func run() (err error) {
	log.SetLevel("info")
	fmt.Println("ready")

	systemAdd := make(chan Filesystem)
	go watchFilesystem(systemAdd)

	ignoreMounting := false

	ctrlc := make(chan os.Signal, 1)
	signal.Notify(ctrlc, os.Interrupt, syscall.SIGTERM, syscall.SIGQUIT, syscall.SIGKILL, syscall.SIGINT)

	for {
		select {
		case <-ctrlc:
			fmt.Println("goodbye")
			os.Remove("pop_.wav")
			os.Exit(0)
		case partition := <-systemAdd:
			if !ignoreMounting {
				ignoreMounting = true
				log.Debugf("found new partition %s", partition.Name)
				go func() {
					time.Sleep(1 * time.Second)
					err = reformatFilesystem(partition)
					if err != nil {
						log.Error(err)
					}
					time.Sleep(1 * time.Second)
					ignoreMounting = false
				}()
			}
		}
	}
}

func playBeep() (err error) {
	// use play to play pop_.wav

	cmd := exec.Command("play", "pop_.wav")
	out, err := cmd.CombinedOutput()
	if err != nil {
		log.Error(err)
	}
	log.Debugf("play: %s", out)
	time.Sleep(1 * time.Second)
	return
}

func main() {
	flag.Parse()
	log.SetLevel(argLogger)

	// create pop_.wav
	err := os.WriteFile("pop_.wav", popWav, 0644)
	if err != nil {
		log.Error(err)
		return
	}
	playBeep()

	// check if the source folder exists
	if _, err := os.Stat(argSourceFolder); os.IsNotExist(err) {
		log.Error("source folder does not exist")
		return
	}
	run()
}
