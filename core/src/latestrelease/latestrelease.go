package latestrelease

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"

	"github.com/google/go-github/v59/github"
	log "github.com/schollz/logger"
)

func download(downloadURL string) (filename string, err error) {
	// Extract filename from the end of the URL
	parts := strings.Split(downloadURL, "/")
	filename = parts[len(parts)-1]

	// Create the file
	out, err := os.Create(filename)
	if err != nil {
		return "", err
	}
	defer out.Close()

	// Get the data
	resp, err := http.Get(downloadURL)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	// Check server response
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("bad status: %s", resp.Status)
	}

	// Write the body to file
	_, err = io.Copy(out, resp.Body)
	if err != nil {
		return "", err
	}

	return filename, nil
}

func DownloadZeptocore() (filename string, err error) {
	downloadURL, err := Zeptocore()
	if err != nil {
		return
	}
	filename, err = download(downloadURL)
	return
}

func Zeptocore() (downloadURL string, err error) {
	tagname, err := get()
	if err != nil {
		return
	}
	downloadURL = fmt.Sprintf("https://github.com/schollz/_core/releases/download/%s/zeptocore_%s.uf2", tagname, tagname)
	return
}

func get() (tagName string, err error) {
	ctx := context.Background()

	client := github.NewClient(nil)

	// Replace "owner" and "repo" with the actual owner and repository name
	owner := "schollz"
	repo := "_core"

	// Fetch the latest release
	release, _, err := client.Repositories.GetLatestRelease(ctx, owner, repo)
	if err != nil {
		log.Errorf("Error fetching the latest release: %v\n", err)
		return
	}

	// Print the latest release tag name and URL
	log.Debugf("Latest release tag: %s\n", *release.TagName)
	tagName = *release.TagName
	return
}
