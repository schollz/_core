package server

import (
	"bytes"
	"embed"
	"fmt"
	"html/template"
	"io"
	"mime"
	"mime/multipart"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"github.com/lucasepe/codename"
	cp "github.com/otiai10/copy"
	"github.com/schollz/_core/core/src/names"
	"github.com/schollz/_core/core/src/onsetdetect"
	"github.com/schollz/_core/core/src/pack"
	"github.com/schollz/_core/core/src/zeptocore"
	log "github.com/schollz/logger"
	bolt "go.etcd.io/bbolt"
)

// embed the static files

//go:embed static/*
var staticFiles embed.FS

var Port = 8101
var StorageFolder = "storage"
var connections map[string]*websocket.Conn
var mutex sync.Mutex
var keystore *bolt.DB
var serverID string
var useFilesOnDisk bool

func Serve(useFiles bool) (err error) {
	useFilesOnDisk = useFiles
	log.Trace("setting up server")
	os.MkdirAll(StorageFolder, 0777)

	log.Trace("opening bolt db")
	keystore, err = bolt.Open(path.Join(StorageFolder, "states.db"), 0600, &bolt.Options{
		Timeout: 1 * time.Second,
	})
	if err != nil {
		return
	}

	// generate a random integer for the session
	rng, err := codename.DefaultRNG()
	if err != nil {
		return
	}
	serverID = codename.Generate(rng, 0)
	log.Tracef("serverID: %s", serverID)

	log.Trace("creating bucket for bolt db")
	err = keystore.Update(func(tx *bolt.Tx) error {
		_, err := tx.CreateBucketIfNotExists([]byte("states"))
		if err != nil {
			return fmt.Errorf("create bucket: %s", err)
		}
		return nil
	})
	if err != nil {
		return
	}

	connections = make(map[string]*websocket.Conn)
	log.Debugf("listening on :%d", Port)
	http.HandleFunc("/", handler)
	err = http.ListenAndServe(fmt.Sprintf(":%d", Port), nil)
	return
}

func handler(w http.ResponseWriter, r *http.Request) {
	t := time.Now().UTC()
	// Redirect URLs with trailing slashes (except for the root "/")
	if r.URL.Path != "/" && strings.HasSuffix(r.URL.Path, "/") {
		http.Redirect(w, r, strings.TrimRight(r.URL.Path, "/"), http.StatusPermanentRedirect)
		return
	}
	err := handle(w, r)
	if err != nil {
		log.Error(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
	log.Debugf("%v %v %v %s\n", r.RemoteAddr, r.Method, r.URL.Path, time.Since(t))
}

func handle(w http.ResponseWriter, r *http.Request) (err error) {
	// w.Header().Set("Access-Control-Allow-Origin", "*")
	// w.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
	// w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization")

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
	} else if r.URL.Path == "/favicon.ico" {
		return handleFavicon(w, r)
	} else {
		// TODO: add caching
		// add caching
		w.Header().Set("Cache-Control", "max-age=86400")
		w.Header().Set("Cache-Control", "no-cache")
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("Cache-Control", "must-revalidate")
		w.Header().Set("Cache-Control", "proxy-revalidate")
		w.Header().Set("Pragma", "no-cache")
		w.Header().Set("Expires", "0")

		filename := r.URL.Path[1:]
		if filename == "" || !strings.Contains(filename, ".") || filename == "buy" || filename == "faq" || filename == "zeptocore" {
			filename = "static/index.html"
		}
		mimeType := mime.TypeByExtension(filepath.Ext(filename))
		w.Header().Set("Content-Type", mimeType)
		var b []byte
		if strings.HasPrefix(filename, StorageFolder) {
			b, err = os.ReadFile(filename)
		} else {
			if useFilesOnDisk {
				filename = path.Join("src/server/", filename)
				b, err = os.ReadFile(filename)
			} else {
				b, err = staticFiles.ReadFile(filename)
			}
		}
		if err != nil {
			log.Errorf("could not read %s: %s", filename, err.Error())
			return
		}

		rng, errR := codename.DefaultRNG()
		if errR != nil {
			err = errR
			return
		}
		serverID = codename.Generate(rng, 0)
		if strings.Contains(filename, "static/index.html") {

			tmpl, errTemplate := template.New("index").Delims("[[", "]]").Parse(string(b))
			if errTemplate != nil {
				log.Errorf("could not parse template %s: %s", filename, errTemplate.Error())
				return
			}

			data := struct {
				IsFaq          bool
				IsBuy          bool
				IsMain         bool
				IsZeptocore    bool
				IsUpload       bool
				VersionCurrent string
				GenURL1        string
				GenURL2        string
			}{
				IsFaq:          r.URL.Path[1:] == "faq",
				IsBuy:          r.URL.Path[1:] == "buy",
				IsMain:         r.URL.Path == "/",
				IsZeptocore:    r.URL.Path == "/zeptocore",
				VersionCurrent: "v1.1.0",
				GenURL1:        codename.Generate(rng, 0),
				GenURL2:        names.Random(),
			}
			data.IsUpload = !(data.IsBuy || data.IsFaq || data.IsMain || data.IsZeptocore)

			err = tmpl.Execute(w, data)
			if err != nil {
				log.Errorf("could not execute template %s: %s", filename, err.Error())
			}
			return
		}
		log.Tracef("serving %s with mime %s", filename, mimeType)
		w.Write(b)
	}

	return
}

func handleFavicon(w http.ResponseWriter, r *http.Request) (err error) {
	w.Header().Set("Content-Type", "image/x-icon")
	var b []byte

	// b, err = os.ReadFile("static/favicon.ico")
	b, err = staticFiles.ReadFile("static/favicon.ico")
	if err != nil {
		return
	}
	w.Write(b)
	return
}

var upgrader = websocket.Upgrader{} // use default options

type Message struct {
	Action     string         `json:"action"`
	Message    string         `json:"message"`
	Boolean    bool           `json:"boolean"`
	Number     int64          `json:"number"`
	Error      string         `json:"error"`
	Success    bool           `json:"success"`
	Filename   string         `json:"filename"`
	Filenames  []string       `json:"filenames"`
	File       zeptocore.File `json:"file"`
	SliceStart []float64      `json:"sliceStart"`
	SliceStop  []float64      `json:"sliceStop"`
	SliceType  []int          `json:"sliceType"`
	State      string         `json:"state"`
	Place      string         `json:"place"`
}

func isValidWorkspace(s string) bool {
	// Define a regular expression for alphanumeric characters
	regex := regexp.MustCompile(`^[a-zA-Z0-9-]+$`)
	return regex.MatchString(s) && !strings.Contains(s, " ")
}

func handleWebsocket(w http.ResponseWriter, r *http.Request) (err error) {
	query := r.URL.Query()
	log.Tracef("query: %+v", query)
	if _, ok := query["id"]; !ok {
		err = fmt.Errorf("no id")
		log.Error(err)
		return
	}
	if _, ok := query["place"]; !ok {
		err = fmt.Errorf("no place")
		log.Error(err)
		return
	}
	place := query["place"][0]

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
		if message.Filename != "" {
			_, message.Filename = filepath.Split(message.Filename)
		}
		log.Tracef("message: %s->%+v", query["id"][0], message.Action)
		if message.Action == "getinfo" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
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
		} else if message.Action == "connected" {
			c.WriteJSON(Message{
				Action:  "connected",
				Message: serverID,
			})
		} else if message.Action == "mergefiles" {
			log.Tracef("message.Filenames: %+v", message.Filenames)
			fnames := make([]string, len(message.Filenames))
			for i, fname := range message.Filenames {
				fnames[i] = path.Join(StorageFolder, place, fname, fname)
			}
			rng, errCode := codename.DefaultRNG()
			if errCode == nil {
				newFilename := codename.Generate(rng, 0) + ".aif"
				os.MkdirAll(path.Join(StorageFolder, place, newFilename), 0777)
				err = Merge(fnames, path.Join(StorageFolder, place, newFilename, newFilename))
				if err != nil {
					log.Error(err)
				} else {
					go processFile(query["id"][0], newFilename, path.Join(StorageFolder, place, newFilename, newFilename))
				}
			}
		} else if message.Action == "onsetdetect" {
			log.Info(message)
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err == nil {
				onsets, err := onsetdetect.OnsetDetect(f.PathToFile+".ogg", int(message.Number))
				if err == nil {
					log.Tracef("onsets: %+v", onsets)
					c.WriteJSON(Message{
						Action:     "onsetdetect",
						SliceStart: onsets,
					})
				} else {
					log.Error(err)
				}
			} else {
				log.Error(err)
			}
		} else if message.Action == "setslices" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			log.Tracef("setting slices: %+v", message.SliceStart)
			if err != nil {
				log.Error(err)
			} else {
				message.SliceType = f.SetSlices(message.SliceStart, message.SliceStop)
				message.Action = "slicetype"
				c.WriteJSON(message)
			}
		} else if message.Action == "setspliceplayback" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetSplicePlayback(int(message.Number))
			}
		} else if message.Action == "setoneshot" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetOneshot(message.Boolean)
			}
		} else if message.Action == "setsplicetrigger" {
			log.Infof("setsplicetrigger: %+v", message)
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetSpliceTrigger(int(message.Number))
			}
		} else if message.Action == "setbpm" {
			log.Infof("setbpm: %+v", message)
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetBPM(int(message.Number))
			}
		} else if message.Action == "setsplicevariable" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetSpliceVariable(message.Boolean)
			}
		} else if message.Action == "setchannels" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				if message.Boolean {
					f.SetChannels(1) // stereo
				} else {
					f.SetChannels(0) // mono
				}
			}
		} else if message.Action == "settempomatch" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetTempoMatch(message.Boolean)
			}
		} else if message.Action == "updatestate" {
			// save into the keystore the message.State for message.Place
			err = keystore.Update(func(tx *bolt.Tx) error {
				b := tx.Bucket([]byte("states"))
				return b.Put([]byte(message.Place), []byte(message.State))
			})
			if err != nil {
				log.Error(err)
			}
		} else if message.Action == "copyworkspace" {
			// copy the workspace into the new place
			log.Infof("copying %s to %s", message.Place, message.Message)
			message.Place = strings.ToLower(message.Place)
			messagePlace := message.Place
			message.Place = strings.TrimPrefix(message.Place, "/")
			if len(message.Place) > 0 {
				if !isValidWorkspace(message.Place) {
					message.Error = fmt.Sprintf("invalid workspace name: %s - no spaces allowed", message.Message)
					log.Error(message.Error)
				} else {
					if _, err = os.Stat(path.Join(StorageFolder, message.Place)); os.IsNotExist(err) {
						message.Error = fmt.Sprintf("folder %s does not exist", message.Place)
						log.Error(message.Error)
					} else {
						if _, err = os.Stat(path.Join(StorageFolder, message.Message)); os.IsNotExist(err) {
							err = cp.Copy(path.Join(StorageFolder, message.Place), path.Join(StorageFolder, message.Message))
							if err != nil {
								message.Error = err.Error()
								log.Error(message.Error)
							} else {
								message.Success = true
								// go through every file in the new storage and change the names
								err = filepath.Walk(path.Join(StorageFolder, message.Message), func(pathName string, info os.FileInfo, err error) error {
									if strings.HasSuffix(pathName, ".json") {
										log.Infof("changing %s", pathName)
									}
									b, _ := os.ReadFile(pathName)
									b = bytes.Replace(b, []byte(path.Join(StorageFolder, message.Place)), []byte(path.Join(StorageFolder, message.Message)), -1)
									os.WriteFile(pathName, b, 0777)
									return nil
								})
								// update the states
								var previousState []byte
								err = keystore.View(func(tx *bolt.Tx) error {
									b := tx.Bucket([]byte("states"))
									previousState = b.Get([]byte(messagePlace))
									if previousState == nil {
										return fmt.Errorf("no state for %s", message.Place)
									}
									return nil
								})
								if err != nil {
									log.Error(err)
								} else {
									err = keystore.Update(func(tx *bolt.Tx) error {
										b := tx.Bucket([]byte("states"))
										return b.Put([]byte("/"+message.Message), previousState)
									})
									if err != nil {
										log.Error(err)
									}

								}
							}
						} else {
							message.Error = fmt.Sprintf("folder %s already exists", message.Message)
							log.Error(message.Error)
						}
					}
				}
			}
			c.WriteJSON(message)
		} else if message.Action == "getstate" {
			// return message.State based for message.Place
			err = keystore.View(func(tx *bolt.Tx) error {
				b := tx.Bucket([]byte("states"))
				v := b.Get([]byte(message.Place))
				if v == nil {
					return fmt.Errorf("no state for %s", message.Place)
				}
				message.State = string(v)
				return nil
			})
			if err != nil {
				log.Error(err)
			} else {
				c.WriteJSON(message)
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
	place := query["place"][0]
	_, place = filepath.Split(place)

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

	zipFile, err := pack.Zip(path.Join(StorageFolder, place), body)
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
	if _, ok := query["place"]; !ok {
		err = fmt.Errorf("no place")
		return
	}
	id := query["id"][0]
	place := query["place"][0]
	_, place = filepath.Split(place)

	// Parse the multipart form data
	err = r.ParseMultipartForm(10 << 20) // 10 MB limit
	if err != nil {
		http.Error(w, "Unable to parse form", http.StatusBadRequest)
		return
	}

	// Retrieve the files from the form data
	files := r.MultipartForm.File["files"]

	// keep track of the total byte count
	totalBytesWritten := int64(0)

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
		localFile := path.Join(StorageFolder, place, file.Filename, file.Filename)
		err = os.MkdirAll(path.Join(StorageFolder, place, file.Filename), 0777)
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
				log.Tracef("n: %d", n)
				mutex.Lock()
				if _, ok := connections[id]; ok {
					connections[id].WriteJSON(Message{
						Action: "progress",
						Number: n + totalBytesWritten,
					})
				}
				mutex.Unlock()
			},
		}

		_, err = io.Copy(byteCounter, uploadedFile)
		if err != nil {
			return
		}

		totalBytesWritten += byteCounter.TotalBytes
		go processFile(id, file.Filename, localFile)
	}

	// Send a response
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"success":true}`))
	return
}

func processFile(id string, uploadedFile string, localFile string) {
	log.Debugf("prcessing file %s from upload %s", localFile, uploadedFile)
	f, err := zeptocore.Get(localFile)
	if err != nil {
		log.Error(err)
		mutex.Lock()
		if _, ok := connections[id]; ok {
			connections[id].WriteJSON(Message{
				Error: fmt.Sprintf("could not process '%s': %s", uploadedFile, err.Error()),
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
}
