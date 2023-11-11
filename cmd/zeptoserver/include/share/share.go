package share

import (
	"compress/gzip"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"sync"
	"time"

	log "github.com/schollz/logger"
)

var ContentDirectory = "uploads"
var MaxBytesPerFile = int64(100000000)

// uploads keep track of parallel chunking
var uploadsLock sync.Mutex
var uploadsInProgress map[string]int
var uploadsFileNames map[string]string
var uploadsHashLock sync.Mutex
var uploadsHash map[string]string

func init() {
	// initialize chunking maps
	uploadsInProgress = make(map[string]int)
	uploadsFileNames = make(map[string]string)
	uploadsHash = make(map[string]string)
}

func HandlePost(w http.ResponseWriter, r *http.Request) (err error) {
	r.ParseMultipartForm(32 << 20)
	file, handler, errForm := r.FormFile("file")
	if errForm != nil {
		err = errForm
		log.Error(err)
		return err
	}
	defer file.Close()
	fname, _ := filepath.Abs(handler.Filename)
	_, fname = filepath.Split(fname)

	log.Debugf("%+v", r.Form)
	chunkNum, _ := strconv.Atoi(r.FormValue("dzchunkindex"))
	chunkNum++
	totalChunks, _ := strconv.Atoi(r.FormValue("dztotalchunkcount"))
	chunkSize, _ := strconv.Atoi(r.FormValue("dzchunksize"))
	if int64(totalChunks)*int64(chunkSize) > MaxBytesPerFile {
		err = fmt.Errorf("Upload exceeds max file size: %d.", MaxBytesPerFile)
		jsonResponse(w, http.StatusBadRequest, map[string]string{"message": err.Error()})
		return nil
	}
	uuid := r.FormValue("dzuuid")
	log.Debugf("working on chunk %d/%d for %s", chunkNum, totalChunks, uuid)

	os.MkdirAll(ContentDirectory, 0755)
	f, err := os.CreateTemp(ContentDirectory, "sharetemp")
	if err != nil {
		log.Error(err)
		return
	}
	// remove temp file when finished
	_, err = CopyMax(f, file, MaxBytesPerFile)
	if err != nil {
		log.Error(err)
	}
	f.Close()

	// check if need to cat
	uploadsLock.Lock()
	if _, ok := uploadsInProgress[uuid]; !ok {
		uploadsInProgress[uuid] = 0
	}
	uploadsInProgress[uuid]++
	uploadsFileNames[fmt.Sprintf("%s%d", uuid, chunkNum)] = f.Name()
	if uploadsInProgress[uuid] == totalChunks {
		err = func() (err error) {
			log.Debugf("upload finished for %s", uuid)
			log.Debugf("%+v", uploadsFileNames)
			delete(uploadsInProgress, uuid)

			fFinal, _ := os.CreateTemp(ContentDirectory, "sharetemp")
			originalSize := int64(0)
			for i := 1; i <= totalChunks; i++ {
				// cat each chunk
				fh, err := os.Open(uploadsFileNames[fmt.Sprintf("%s%d", uuid, i)])
				delete(uploadsFileNames, fmt.Sprintf("%s%d", uuid, i))
				if err != nil {
					log.Error(err)
					return err
				}
				n, errCopy := io.Copy(fFinal, fh)
				originalSize += n
				if errCopy != nil {
					log.Error(errCopy)
				}
				fh.Close()
				log.Debugf("removed %s", fh.Name())
				os.Remove(fh.Name())
			}
			fFinal.Close()
			log.Debugf("final written to: %s", fFinal.Name())
			os.Rename(fFinal.Name(), filepath.Join(ContentDirectory, fname))
			log.Debugf("setting uploadsHash: %s", fname)
			uploadsHashLock.Lock()
			uploadsHash[uuid] = fname
			uploadsHashLock.Unlock()
			return
		}()
	}
	uploadsLock.Unlock()

	if err != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{"message": err.Error()})
		return nil
	}

	// wait until all are finished
	var finalFname string
	startTime := time.Now()
	for {
		uploadsHashLock.Lock()
		if _, ok := uploadsHash[uuid]; ok {
			finalFname = uploadsHash[uuid]
			log.Debugf("got uploadsHash: %s", finalFname)
		}
		uploadsHashLock.Unlock()
		if finalFname != "" {
			break
		}
		time.Sleep(100 * time.Millisecond)
		if time.Since(startTime).Seconds() > 60*60 {
			break
		}
	}

	jsonResponse(w, http.StatusCreated, map[string]string{"id": finalFname})
	return
}

// CopyMax copies only the maxBytes and then returns an error if it
// copies equal to or greater than maxBytes (meaning that it did not
// complete the copy).
func CopyMax(dst io.Writer, src io.Reader, maxBytes int64) (n int64, err error) {
	n, err = io.CopyN(dst, src, maxBytes)
	if err != nil && err != io.EOF {
		return
	}

	if n >= maxBytes {
		err = fmt.Errorf("Upload exceeds maximum size (%s).", MaxBytesPerFile)
	} else {
		err = nil
	}
	return
}

// jsonResponse writes a JSON response and HTTP code
func jsonResponse(w http.ResponseWriter, code int, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json, err := json.Marshal(data)
	if err != nil {
		log.Error(err)
	}
	log.Debugf("json response: %s", json)
	fmt.Fprintf(w, "%s\n", json)
}

// writeAllBytes takes a reader and writes it to the content directory.
// It throws an error if the number of bytes written exceeds what is set.
func writeAllBytes(fname string, src io.Reader) (fnameFull string, err error) {
	f, err := os.CreateTemp(ContentDirectory, "sharetemp")
	if err != nil {
		log.Error(err)
		return
	}
	// remove temp file when finished
	defer os.Remove(f.Name())
	w := gzip.NewWriter(f)

	// try to write the bytes
	n, err := CopyMax(w, src, MaxBytesPerFile)
	w.Flush()
	w.Close()
	f.Close()

	// if an error occured, then erase the temp file
	if err != nil {
		os.Remove(f.Name())
		log.Error(err)
		return
	} else {
		log.Debugf("wrote %d bytes to %s", n, f.Name())
	}
	return
}
