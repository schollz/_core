package server

import (
	"bytes"
	"embed"
	"encoding/json"
	"fmt"
	"html/template"
	"io"
	"mime"
	"mime/multipart"
	"net/http"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	cp "github.com/otiai10/copy"
	"github.com/schollz/_core/core/src/detectdisks"
	"github.com/schollz/_core/core/src/drumextract2"
	"github.com/schollz/_core/core/src/latestrelease"
	"github.com/schollz/_core/core/src/names"
	"github.com/schollz/_core/core/src/onsetdetect"
	"github.com/schollz/_core/core/src/pack"
	"github.com/schollz/_core/core/src/utils"
	"github.com/schollz/_core/core/src/zeptocore"
	"github.com/schollz/codename"
	log "github.com/schollz/logger"
)

// embed the static files

//go:embed static/*
var staticFiles embed.FS

//go:embed docs/*
var docsFiles embed.FS

// mutex for reading/writing storage files
var mutexStorage sync.Mutex

var Port = 8101
var StorageFolder = "storage"
var connections map[string]*websocket.Conn
var activeDebounce map[string]chan bool
var mutex sync.Mutex
var serverID string
var useFilesOnDisk bool
var isPluggedIn bool
var chanPrepareUpload chan bool
var chanDeviceType chan string
var chanString chan string
var latestTag string
var deviceVersion string
var deviceType string
var isEctocore bool

func stateSave(place string, state string) (err error) {
	mutexStorage.Lock()
	defer mutexStorage.Unlock()
	// save state by writing to file
	// remove beginning slash if it exists in place
	place = strings.TrimPrefix(place, "/")
	err = os.WriteFile(path.Join(StorageFolder, "states", place+".json"), []byte(state), 0777)
	return
}

func stateLoad(place string) (state string, err error) {
	mutexStorage.Lock()
	defer mutexStorage.Unlock()
	// load state by reading from file
	// remove beginning slash if it exists in place
	place = strings.TrimPrefix(place, "/")
	stateBytes, err := os.ReadFile(path.Join(StorageFolder, "states", place+".json"))
	if err != nil {
		return
	}
	state = string(stateBytes)
	return
}

func Serve(useEctocore bool, useFiles bool, flagDontConnect bool, chanStringArg chan string, chanPrepareUploadArg chan bool, chanDeviceTypeArg chan string) (err error) {
	isEctocore = useEctocore
	useFilesOnDisk = useFiles
	chanPrepareUpload = chanPrepareUploadArg
	chanDeviceType = chanDeviceTypeArg
	chanString = chanStringArg
	log.Trace("setting up server")
	os.MkdirAll(StorageFolder, 0777)

	// generate a random integer for the session
	rng, err := codename.DefaultRNG()
	if err != nil {
		return
	}
	serverID = codename.Generate(rng, 0)
	log.Tracef("serverID: %s", serverID)

	// make keystore folder if it doesn't exist
	os.MkdirAll(path.Join(StorageFolder, "states"), 0777)

	if !flagDontConnect {
		go func() {
			for {
				select {
				case deviceType = <-chanDeviceType:
					log.Debugf("deviceType: %v", deviceType)
					mutex.Lock()
					for _, c := range connections {
						c.WriteJSON(Message{
							Action:        "devicefound",
							DeviceType:    deviceType,
							DeviceVersion: deviceVersion,
							LatestVersion: latestTag,
						})
					}
					mutex.Unlock()
				case s := <-chanString:
					s = strings.TrimSpace(s)
					log.Debugf("minicom: %s", s)
					if strings.HasPrefix(s, "version=") {
						deviceVersion = strings.TrimPrefix(s, "version=")
						log.Debugf("device version: %s", deviceVersion)

						mutex.Lock()
						for _, c := range connections {
							c.WriteJSON(Message{
								Action:        "devicefound",
								DeviceType:    deviceType,
								DeviceVersion: deviceVersion,
								LatestVersion: latestTag,
							})
						}
						mutex.Unlock()
					}
					log.Tracef("minicom: %s", s)
				}

			}
		}()

	}

	go func() {
		latestTag, _ = latestrelease.Tag()
		log.Debugf("latest tag: %s", latestTag)
	}()

	connections = make(map[string]*websocket.Conn)
	activeDebounce = make(map[string]chan bool)
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
	log.Tracef("%v %v %v %s\n", r.RemoteAddr, r.Method, r.URL.Path, time.Since(t))
}

func handle(w http.ResponseWriter, r *http.Request) (err error) {

	// very special paths
	if r.URL.Path == "/drumextract" {
		return handleDrumExtract(w, r)
	} else if r.URL.Path == "/onsetdetect" {
		return handleOnsetDetect(w, r)
	} else if r.Method == "POST" {
		// POST file
		if r.URL.Path == "/download" {
			return handleDownload(w, r)
		} else {
			return handleUpload(w, r)
		}
	} else if r.URL.Path == "/get_info" {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
		w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization")

		b, _ := json.Marshal(map[string]string{"version": "v7.1.1"})
		w.Write(b)
		return nil
	} else if r.URL.Path == "/ws" {
		return handleWebsocket(w, r)
	} else if r.URL.Path == "/robots.txt" {
		w.Header().Set("Content-Type", "text/plain")
		w.Write([]byte("User-agent: *\nAllow: /\n"))
		return
	} else if r.URL.Path == "/favicon.ico" {
		return handleFavicon(w, r)
	} else if !isEctocore &&
		(strings.HasPrefix(r.URL.Path, "/docs") ||
			strings.Contains(r.URL.Path, "buy") ||
			strings.HasPrefix(r.URL.Path, "/guide") ||
			r.URL.Path == "/") {
		if strings.Contains(r.URL.Path, "buy") || r.URL.Path == "/docs" || r.URL.Path == "/docs/" || r.URL.Path == "/guide" || r.URL.Path == "/guide/" || r.URL.Path == "/" {
			r.URL.Path = "/docs/index.html"
		}
		var b []byte
		b, err = docsFiles.ReadFile(strings.TrimPrefix(r.URL.Path, "/"))
		if err != nil {
			return
		}
		w.Write(b)
		return
	} else {

		// add caching
		w.Header().Set("Cache-Control", "max-age=86400")
		w.Header().Set("Expires", time.Now().Add(86400*time.Second).Format(time.RFC1123))
		w.Header().Set("Last-Modified", time.Now().Format(time.RFC1123))
		w.Header().Set("Pragma", "cache")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("X-XSS-Protection", "1; mode=block")

		if strings.HasPrefix(r.URL.Path, "/tool") {
			r.URL.Path = "/" + strings.TrimPrefix(r.URL.Path, "/tool")
		}

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
				if err != nil {
					b, err = docsFiles.ReadFile(path.Join("docs", filename))
				}
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
				IsEctocore     bool
				IsUpload       bool
				VersionCurrent string
				LatestVersion  string
				GenURL1        string
				GenURL2        string
			}{
				IsMain:         r.URL.Path == "/",
				VersionCurrent: "v7.1.1",
				LatestVersion:  latestTag,
				GenURL1:        codename.Generate(rng, 0),
				GenURL2:        names.Random(),
				IsEctocore:     isEctocore,
				IsZeptocore:    !isEctocore,
			}
			data.IsUpload = !(data.IsMain)

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
	Action               string         `json:"action"`
	Message              string         `json:"message"`
	Boolean              bool           `json:"boolean"`
	Number               int64          `json:"number"`
	Error                string         `json:"error"`
	Success              bool           `json:"success"`
	Filename             string         `json:"filename"`
	Filenames            []string       `json:"filenames"`
	File                 zeptocore.File `json:"file"`
	SliceStart           []float64      `json:"sliceStart"`
	SliceStop            []float64      `json:"sliceStop"`
	SliceType            []int          `json:"sliceType"`
	State                string         `json:"state"`
	Place                string         `json:"place"`
	DeviceType           string         `json:"deviceType"`
	DeviceVersion        string         `json:"deviceVersion"`
	DeviceFirmwareUpload string         `json:"deviceFirmwareUpload"`
	LatestVersion        string         `json:"latestVersion"`
	Transients           [][]int        `json:"transients"`
	FileNum              int            `json:"fileNum"`
	BankNum              int            `json:"bankNum"`
	I                    int            `json:"i"`
	J                    int            `json:"j"`
	N                    int            `json:"n"`
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
		connID := query["id"][0]
		if _, ok := connections[connID]; ok {
			delete(connections, connID)
		}
		// Cancel any active debounce operation for this connection
		if cancelChan, exists := activeDebounce[connID]; exists {
			close(cancelChan)
			delete(activeDebounce, connID)
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
		} else if message.Action == "isprocessing" {
			// Cancel any existing debounce operation for this connection
			mutex.Lock()
			connID := query["id"][0]
			if cancelChan, exists := activeDebounce[connID]; exists {
				close(cancelChan)
				delete(activeDebounce, connID)
			}
			// Create a new cancel channel for this debounce operation
			cancelChan := make(chan bool)
			activeDebounce[connID] = cancelChan
			mutex.Unlock()

			// Implement debouncing: check for file modifications, wait 3 seconds,
			// and keep waiting until 3 seconds pass with no new modifications
			go func() {
				defer func() {
					mutex.Lock()
					delete(activeDebounce, connID)
					mutex.Unlock()
				}()

				folder := path.Join(StorageFolder, place)

				// Get initial set of recently modified files (within last 3 seconds)
				checkTime := time.Now().Add(-3 * time.Second)
				modifiedFiles, err := utils.GetRecentlyModifiedFiles(folder, checkTime)

				if err != nil || len(modifiedFiles) == 0 {
					// No recent modifications
					mutex.Lock()
					if _, ok := connections[connID]; ok {
						connections[connID].WriteJSON(Message{
							Action: "notworking",
						})
					}
					mutex.Unlock()
					return
				}

				// Files are being modified, enter debouncing loop
				mutex.Lock()
				if _, ok := connections[connID]; ok {
					connections[connID].WriteJSON(Message{
						Action: "isworking",
					})
				}
				mutex.Unlock()

				// Find the most recent modification time
				var lastModTime time.Time
				for _, modTime := range modifiedFiles {
					if modTime.After(lastModTime) {
						lastModTime = modTime
					}
				}

				// Debouncing loop: wait 3 seconds and check for new modifications
				for {
					select {
					case <-cancelChan:
						// This debounce was cancelled by a new isprocessing request
						return
					case <-time.After(3 * time.Second):
						// Check if any files were modified since last check
						newModifiedFiles, err := utils.GetRecentlyModifiedFiles(folder, lastModTime)
						if err != nil || len(newModifiedFiles) == 0 {
							// No new modifications in the last 3 seconds, work is done
							mutex.Lock()
							if _, ok := connections[connID]; ok {
								connections[connID].WriteJSON(Message{
									Action: "notworking",
								})
							}
							mutex.Unlock()
							return
						}

						// Update last modification time
						for _, modTime := range newModifiedFiles {
							if modTime.After(lastModTime) {
								lastModTime = modTime
							}
						}
						// Continue waiting (loop will repeat)
					}
				}
			}()
		} else if message.Action == "uploadfirmware" {
			var success bool
			var message string
			log.Debug("uploading firmware")

			c.WriteJSON(Message{
				Action:               "firmwareprogress",
				DeviceFirmwareUpload: "uploading",
			})
			chanPrepareUpload <- true
			uf2disk := ""
			for i := 0; i < 10; i++ {
				time.Sleep(1 * time.Second)
				uf2disk, err = detectdisks.GetUF2Drive()
				if err == nil {
					break
				}
			}
			if uf2disk != "" {
				c.WriteJSON(Message{
					Action:               "firmwareprogress",
					DeviceFirmwareUpload: "download latest",
				})

				log.Debug("found disk")
				var downloadedUF2 string
				downloadedUF2, err = latestrelease.Download(deviceType)
				if err != nil {
					message = err.Error()
					log.Error(err)
				} else {
					// copy the file to the disk
					destinationFile := path.Join(uf2disk, "firmware.uf2")
					c.WriteJSON(Message{
						Action:               "firmwareprogress",
						DeviceFirmwareUpload: "copying to " + uf2disk,
					})
					log.Debugf("copying file to disk: %s->%s", downloadedUF2, destinationFile)
					err = utils.CopyFile(downloadedUF2, destinationFile)
					if err != nil {
						message = err.Error()
						log.Error(err)
					} else {
						log.Debug("copied file to disk")
						success = true
					}
				}
			} else {
				log.Error("could not find disk")
				message = "could not find disk"
			}
			if !success {
				message = "could not upload firmware: " + message
				message += ". please try unplugging and replugging the device and try again."
			} else {
				c.WriteJSON(Message{
					Action:               "firmwareprogress",
					DeviceFirmwareUpload: "reloading",
				})
			}
			c.WriteJSON(Message{
				Action:  "firmwareuploaded",
				Success: success,
				Message: message,
			})
		} else if message.Action == "connected" {
			c.WriteJSON(Message{
				Action:  "connected",
				Message: serverID,
			})
			c.WriteJSON(Message{
				Action:        "devicefound",
				DeviceType:    deviceType,
				DeviceVersion: deviceVersion,
				LatestVersion: latestTag,
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
					go processFile(query["id"][0], newFilename, path.Join(StorageFolder, place, newFilename, newFilename), "default")
				}
			}
		} else if message.Action == "onsetdetect" {
			go func() {
				log.Info(message)
				f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
				if err == nil {
					_, errAubioOnset := exec.LookPath("aubioonset")
					var onsets []float64
					if errAubioOnset == nil {
						onsets, err = onsetdetect.OnsetDetect(f.PathToFile+".ogg", int(message.Number))
					} else {
						onsets, err = onsetdetect.OnsetDetectAPI(f.PathToFile+".ogg", int(message.Number))
					}
					if len(onsets) > 120 {
						onsets = onsets[:120]
					}
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
			}()
		} else if message.Action == "gettransients" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err == nil {
				isEmpty := true
				for _, v := range f.Transients {
					for _, vv := range v {
						if vv != 0 {
							isEmpty = false
							break
						}
					}
					if !isEmpty {
						break
					}
				}
				if !isEmpty {
					// send the transients
					c.WriteJSON(Message{
						Action:     "transients",
						Transients: f.Transients,
						BankNum:    message.BankNum,
						FileNum:    message.FileNum,
					})
				}

			}
		} else if message.Action == "settransient" {
			f, err := zeptocore.Get(path.Join(StorageFolder, place, message.Filename, message.Filename))
			if err != nil {
				log.Error(err)
			} else {
				f.SetTransient(message.I, message.J, message.N)
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
			// log.Debugf("updating state for %s: %s", message.Place, message.State)
			err = stateSave(message.Place, message.State)
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
								stateString, err := stateLoad(messagePlace)
								if err != nil {
									log.Error(err)
								} else {
									err = stateSave(message.Message, stateString)
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
			message.State, err = stateLoad(message.Place)
			if err != nil {
				log.Trace(err)
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
	settingsOnlyString := query.Get("settingsOnly")
	settingsOnly := settingsOnlyString == "true"

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

	zipFile, err := pack.Zip(path.Join(StorageFolder, place), body, settingsOnly)
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

const maxUploadSize = 10 * 1024 * 1024 // 10 MB

func handleOnsetDetect(w http.ResponseWriter, r *http.Request) (err error) {
	if r.Method != http.MethodPut {
		log.Error(err)
		return
	}
	// get query number
	query := r.URL.Query()
	if _, ok := query["number"]; !ok {
		err = fmt.Errorf("no number")
		return
	}
	numberString := query["number"][0]
	number, err := strconv.ParseInt(numberString, 10, 64)
	if err != nil {
		log.Error(err)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, maxUploadSize)

	folder := path.Join(StorageFolder, "onsetdetect")
	// create folder if it doesn't exist
	os.MkdirAll(folder, 0777)
	// create a random file name
	filename := path.Join(folder, utils.RandomString(16)+".ogg")

	out, err := os.Create(filename)
	if err != nil {
		log.Error(err)
		return
	}
	defer out.Close()

	_, err = io.Copy(out, r.Body)
	if err != nil {
		log.Error(err)
		return
	}

	a, err := onsetdetect.OnsetDetect(filename, int(number))
	if err != nil {
		log.Error(err)
		return
	}
	// return json with a, b, c
	jsonResponse, _ := json.Marshal(a)
	w.Header().Set("Content-Type", "application/json")
	w.Write(jsonResponse)
	return
}

func handleDrumExtract(w http.ResponseWriter, r *http.Request) (err error) {
	if r.Method != http.MethodPut {
		log.Error(err)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, maxUploadSize)

	folder := path.Join(StorageFolder, "drumextract")
	// create folder if it doesn't exist
	os.MkdirAll(folder, 0777)
	// create a random file name
	filename := path.Join(folder, utils.RandomString(16)+".ogg")

	out, err := os.Create(filename)
	if err != nil {
		log.Error(err)
		return
	}
	defer out.Close()

	_, err = io.Copy(out, r.Body)
	if err != nil {
		log.Error(err)
		return
	}

	filename2, err := utils.HashFile(filename)
	if err != nil {
		log.Error(err)
		return
	}
	filename2 = path.Join(folder, filename2+".ogg")
	log.Debugf("renaming %s to %s", filename, filename2)
	err = os.Rename(filename, filename2)
	if err != nil {
		log.Error(err)
		return
	}

	a, b, c, err := drumextract2.DrumExtract2(filename2)
	log.Debugf("a: %v, b: %v, c: %v, err: %v", a, b, c, err)
	// return json with a, b, c
	jsonResponse, _ := json.Marshal(struct {
		A []int `json:"a"`
		B []int `json:"b"`
		C []int `json:"c"`
	}{a, b, c})
	w.Header().Set("Content-Type", "application/json")
	w.Write(jsonResponse)
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
	if _, ok := query["dropaudiofilemode"]; !ok {
		err = fmt.Errorf("no dropaudiofilemode")
		return
	}
	id := query["id"][0]
	place := query["place"][0]
	dropaudiofilemode := query["dropaudiofilemode"][0]

	log.Debugf("upload, %s, %s, %s", id, place, dropaudiofilemode)
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
				// mutex.Lock()
				// if _, ok := connections[id]; ok {
				// 	connections[id].WriteJSON(Message{
				// 		Action: "progress",
				// 		Number: n + totalBytesWritten,
				// 	})
				// }
				// mutex.Unlock()
			},
		}

		_, err = io.Copy(byteCounter, uploadedFile)
		if err != nil {
			return
		}

		totalBytesWritten += byteCounter.TotalBytes
		go processFile(id, file.Filename, localFile, dropaudiofilemode)
	}

	// Send a response
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"success":true}`))
	return
}

func processFile(id string, uploadedFile string, localFile string, dropaudiofilemode string) {
	log.Debugf("prcessing file %s from upload %s (%s)", localFile, uploadedFile, dropaudiofilemode)
	f, err := zeptocore.Get(localFile, dropaudiofilemode)
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
