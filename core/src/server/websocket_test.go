package server

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gorilla/websocket"
	"github.com/stretchr/testify/require"
)

func TestClosingReplacedWebsocketPreservesUploadNotifications(t *testing.T) {
	previousConnections, previousDebounce := connections, activeDebounce
	connections = make(map[string]*websocket.Conn)
	activeDebounce = make(map[string]chan bool)
	t.Cleanup(func() {
		connections, activeDebounce = previousConnections, previousDebounce
	})

	finished := make(chan struct{}, 2)
	var handlers sync.WaitGroup
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		handlers.Add(1)
		defer handlers.Done()
		defer func() { finished <- struct{}{} }()
		if err := handleWebsocket(w, r); err != nil {
			t.Errorf("websocket handler: %v", err)
		}
	}))
	t.Cleanup(func() {
		server.Close()
		handlers.Wait()
	})

	waitForClose := func() {
		t.Helper()
		select {
		case <-finished:
		case <-time.After(5 * time.Second):
			t.Fatal("websocket handler did not finish")
		}
	}
	connect := func() *websocket.Conn {
		t.Helper()
		mutex.Lock()
		previous := connections["upload-client"]
		mutex.Unlock()
		url := "ws" + strings.TrimPrefix(server.URL, "http") + "/ws?id=upload-client&place=/workspace"
		client, _, err := websocket.DefaultDialer.Dial(url, nil)
		if err != nil {
			t.Fatal(err)
		}
		t.Cleanup(func() { client.Close() })
		client.SetReadDeadline(time.Now().Add(5 * time.Second))
		// The upgrade handshake completes just before registration. Wait for the
		// new entry without introducing an unrelated server-side writer.
		require.Eventually(t, func() bool {
			mutex.Lock()
			defer mutex.Unlock()
			return connections["upload-client"] != nil && connections["upload-client"] != previous
		}, 5*time.Second, time.Millisecond, "server did not register the connection")
		return client
	}

	older := connect()
	current := connect()
	pendingWork := make(chan bool)
	mutex.Lock()
	activeDebounce["upload-client"] = pendingWork
	mutex.Unlock()

	older.Close()
	waitForClose()
	select {
	case <-pendingWork:
		t.Fatal("closing the old connection cancelled the current connection's work")
	default:
	}

	// Upload completion is delivered through this registry, not the request socket.
	mutex.Lock()
	registered := connections["upload-client"]
	mutex.Unlock()
	if registered == nil {
		t.Fatal("closing the old connection unregistered the current connection")
	}
	if err := registered.WriteJSON(Message{Action: "processed", Filename: "sample.wav"}); err != nil {
		t.Fatal(err)
	}
	var notification Message
	if err := current.ReadJSON(&notification); err != nil {
		t.Fatal(err)
	}
	if notification.Action != "processed" || notification.Filename != "sample.wav" {
		t.Fatalf("unexpected upload notification: %+v", notification)
	}

	current.Close()
	waitForClose()
	mutex.Lock()
	_, connected := connections["upload-client"]
	_, working := activeDebounce["upload-client"]
	mutex.Unlock()
	if connected || working {
		t.Fatal("closing the current connection left its connection or work registered")
	}
	select {
	case <-pendingWork:
	default:
		t.Fatal("closing the current connection did not cancel its pending work")
	}
}
