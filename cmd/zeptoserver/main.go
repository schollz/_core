package main

import (
	"context"
	"fmt"
	"io/ioutil"
	"net/http"
	"time"

	log "github.com/schollz/logger"
	"github.com/schollz/websocket"
	"github.com/schollz/websocket/wsjson"
)

func main() {
	log.SetLevel("debug")
	port := 8098
	log.Infof("listening on :%d", port)
	http.HandleFunc("/", handler)
	http.ListenAndServe(fmt.Sprintf(":%d", port), nil)
}

func handler(w http.ResponseWriter, r *http.Request) {
	t := time.Now().UTC()
	err := handle(w, r)
	if err != nil {
		log.Error(err)
	}
	log.Infof("%v %v %v %s\n", r.RemoteAddr, r.Method, r.URL.Path, time.Since(t))
}

func handle(w http.ResponseWriter, r *http.Request) (err error) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
	w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, X-CSRF-Token, Authorization")

	// very special paths
	if r.URL.Path == "/ws" {
		return handleWebsocket(w, r)
	} else {
		b, _ := ioutil.ReadFile("index.html")
		w.Write(b)
	}

	return
}

func handleWebsocket(w http.ResponseWriter, r *http.Request) (err error) {
	c, err := websocket.Accept(w, r, nil)
	if err != nil {
		return
	}
	defer c.Close(websocket.StatusInternalError, "internal error")

	ctx, cancel := context.WithTimeout(r.Context(), time.Hour*120000)
	defer cancel()

	for {
		var v interface{}
		err = wsjson.Read(ctx, c, &v)
		if err != nil {
			break
		}
		log.Debugf("received: %v", v)
		err = wsjson.Write(ctx, c, struct{ Message string }{
			"hello, browser",
		})
		if err != nil {
			break
		}
	}
	if websocket.CloseStatus(err) == websocket.StatusGoingAway {
		err = nil
	}
	c.Close(websocket.StatusNormalClosure, "")
	return
}
