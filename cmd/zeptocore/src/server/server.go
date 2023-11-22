package server

import (
	"fmt"
	"io"
	"mime"
	"mime/multipart"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	log "github.com/schollz/logger"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/pack"
	"github.com/schollz/zeptocore/cmd/zeptocore/src/zeptocore"
)

var Port = 8101
var StorageFolder = "storage"
var connections map[string]*websocket.Conn
var mutex sync.Mutex

func Serve() {
	os.MkdirAll(StorageFolder, 0777)
	connections = make(map[string]*websocket.Conn)
	log.Infof("listening on :%d", Port)
	http.HandleFunc("/", handler)
	http.ListenAndServe(fmt.Sprintf(":%d", Port), nil)
}

func handler(w http.ResponseWriter, r *http.Request) {
	t := time.Now().UTC()
	err := handle(w, r)
	if err != nil {
		log.Error(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
	log.Infof("%v %v %v %s\n", r.RemoteAddr, r.Method, r.URL.Path, time.Since(t))
}

func handle(w http.ResponseWriter, r *http.Request) (err error) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization")

	// very special paths
	if r.Method == "POST" {
		// POST file
		if r.URL.Path == "/download" {
			return handleDownload(w, r)
		} else {
			return handleUpload(w, r)
		}
	} else if r.URL.Path == "/ws" {
		return handleWebsocket(w, r)
	} else {
		if r.URL.Path == "/" {
			r.URL.Path = "/index.html"
		}
		mime := mime.TypeByExtension(filepath.Ext(r.URL.Path))
		w.Header().Set("Content-Type", mime)
		var b []byte
		b, err = os.ReadFile(r.URL.Path[1:])
		if err != nil {
			return
		}
		w.Write(b)
	}

	return
}

var upgrader = websocket.Upgrader{} // use default options

type Message struct {
	Action     string         `json:"action"`
	Message    string         `json:"message"`
	Number     int64          `json:"number"`
	Error      string         `json:"error"`
	Success    bool           `json:"success"`
	Filename   string         `json:"filename"`
	File       zeptocore.File `json:"file"`
	SliceStart []float64      `json:"sliceStart"`
	SliceStop  []float64      `json:"sliceStop"`
}

func handleWebsocket(w http.ResponseWriter, r *http.Request) (err error) {
	query := r.URL.Query()
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		return
	}

	// use gorilla to open websocket
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	defer func() {
		c.Close()
		mutex.Lock()
		if _, ok := connections[query["id"][0]]; ok {
			delete(connections, query["id"][0])
		}
		mutex.Unlock()
	}()
	mutex.Lock()
	connections[query["id"][0]] = c
	mutex.Unlock()

	for {
		var message Message
		err := c.ReadJSON(&message)
		if err != nil {
			break
		}
		log.Debugf("message: %+v", message)
		if message.Action == "getinfo" {
			f, err := zeptocore.Get(message.Filename)
			if err != nil {
				c.WriteJSON(Message{
					Action: "error",
					Error:  err.Error(),
				})
			} else {
				c.WriteJSON(Message{
					Action:   "setinfo",
					Filename: message.Filename,
					File:     f,
					Success:  true,
				})
			}
		} else if message.Action == "setslices" {
			f, err := zeptocore.Get(message.Filename)
			if err != nil {
				log.Error(err)
			} else {
				f.SetSlices(message.SliceStart, message.SliceStop)
			}
		}
	}
	return
}

type ByteCounter struct {
	Callback     func(int64)
	TotalBytes   int64
	TargetWriter io.Writer
}

func (bc *ByteCounter) Write(p []byte) (n int, err error) {
	n, err = bc.TargetWriter.Write(p)
	bc.TotalBytes += int64(n)
	bc.Callback(bc.TotalBytes)
	return n, err
}

func handleDownload(w http.ResponseWriter, r *http.Request) (err error) {
	// get the url query parameters from the request r
	query := r.URL.Query()
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		return
	}
	id := query["id"][0]

	mutex.Lock()
	if _, ok := connections[id]; ok {
		connections[id].WriteJSON(Message{
			Action: "processingstart",
		})
	}
	mutex.Unlock()

	defer func() {
		mutex.Lock()
		if _, ok := connections[id]; ok {
			connections[id].WriteJSON(Message{
				Action: "processingstop",
			})
		}
		mutex.Unlock()

	}()

	// Read the JSON data from the request body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		log.Error(err)
		return
	}

	zipFile, err := pack.Zip(StorageFolder, body)
	if err != nil {
		log.Error(err)
		return
	}

	log.Tracef("zipFile: %s", zipFile)

	_, fname := filepath.Split(zipFile)

	// Set the Content-Disposition header to trigger download
	w.Header().Set("Content-Disposition", "attachment; filename="+fname)
	w.Header().Set("Content-Type", "application/zip")

	// Serve the ZIP file
	http.ServeFile(w, r, zipFile)
	return
}

func handleUpload(w http.ResponseWriter, r *http.Request) (err error) {
	// get the url query parameters from the request r
	query := r.URL.Query()
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		return
	}
	id := query["id"][0]

	// Parse the multipart form data
	err = r.ParseMultipartForm(10 << 20) // 10 MB limit
	if err != nil {
		http.Error(w, "Unable to parse form", http.StatusBadRequest)
		return
	}

	// Retrieve the files from the form data
	files := r.MultipartForm.File["files"]

	// Process each file
	for _, file := range files {
		// Open the uploaded file
		var uploadedFile multipart.File
		uploadedFile, err = file.Open()
		if err != nil {
			return
		}
		defer uploadedFile.Close()

		// Read the file content
		// save file locally
		localFile := path.Join(StorageFolder, file.Filename, file.Filename)
		err = os.MkdirAll(path.Join(StorageFolder, file.Filename), 0777)
		if err != nil {
			return
		}
		var destination *os.File
		destination, err = os.Create(localFile)
		if err != nil {
			return
		}
		byteCounter := &ByteCounter{
			TargetWriter: destination,
			Callback: func(n int64) {
				log.Debugf("n: %d", n)
				mutex.Lock()
				if _, ok := connections[id]; ok {
					connections[id].WriteJSON(Message{
						Action: "progress",
						Number: n,
					})
				}
				mutex.Unlock()
			},
		}

		_, err = io.Copy(byteCounter, uploadedFile)
		if err != nil {
			return
		}

		go func(uploadedFile string, localFile string) {
			log.Debugf("prcessing file %s from upload %s", localFile, uploadedFile)
			f, err := zeptocore.Get(localFile)
			if err != nil {
				log.Error(err)
				mutex.Lock()
				if _, ok := connections[id]; ok {
					connections[id].WriteJSON(Message{
						Error: err.Error(),
					})
				}
				mutex.Unlock()
				return
			}

			mutex.Lock()
			if _, ok := connections[id]; ok {
				connections[id].WriteJSON(Message{
					Action:   "processed",
					Filename: uploadedFile,
					File:     f,
				})
			}
			mutex.Unlock()
		}(file.Filename, localFile)
	}

	// Send a response
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"success":true}`))
	return
}
