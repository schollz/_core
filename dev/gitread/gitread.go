package main

import (
	"log"
	"os"
	"path"
	"sort"
	"strings"
	"text/template"
	"time"

	git "github.com/go-git/go-git/v5"
	"github.com/go-git/go-git/v5/plumbing"
	"github.com/go-git/go-git/v5/plumbing/object"
	"golang.org/x/mod/semver"
)

type CommitInfo struct {
	Message string
	Hash    string
}

type VersionInfo struct {
	Version   string
	Timestamp time.Time
	Commits   []CommitInfo
}

type TemplateData struct {
	Versions []VersionInfo
}

func main() {
	r, err := git.PlainOpen("../..")
	if err != nil {
		log.Fatal(err)
	}

	// Retrieve all tags (versions) in the repository
	tagRefs, err := r.Tags()
	if err != nil {
		log.Fatal(err)
	}

	// Store the tags in a slice and sort them by semver
	var tags []*plumbing.Reference
	err = tagRefs.ForEach(func(ref *plumbing.Reference) error {
		tags = append(tags, ref)
		return nil
	})
	if err != nil {
		log.Fatal(err)
	}

	sort.Slice(tags, func(i, j int) bool {
		return semver.Compare(tags[i].Name().Short(), tags[j].Name().Short()) > 0
	})

	var data TemplateData

	// Iterate over tags to get commits between versions
	for i := 0; i < len(tags)-1; i++ {
		currentTag := tags[i]
		nextTag := tags[i+1]

		currentTagCommit, err := r.CommitObject(currentTag.Hash())
		if err != nil {
			log.Fatal(err)
		}

		nextTagCommit, err := r.CommitObject(nextTag.Hash())
		if err != nil {
			log.Fatal(err)
		}

		cIter, err := r.Log(&git.LogOptions{From: currentTagCommit.Hash})
		if err != nil {
			log.Fatal(err)
		}

		versionInfo := VersionInfo{
			Version:   currentTag.Name().Short(),
			Timestamp: currentTagCommit.Author.When,
		}

		inCurrentVersion := false
		err = cIter.ForEach(func(c *object.Commit) error {
			if c.Hash == currentTagCommit.Hash {
				inCurrentVersion = true
				return nil
			}
			if c.Hash == nextTagCommit.Hash {
				inCurrentVersion = false
				return nil
			}
			if !inCurrentVersion {
				return nil
			}
			if strings.Contains(c.Message, "Former-commit-id") {
				return nil
			}
			messageFields := strings.Fields(c.Message)
			message := strings.Join(messageFields, " ")
			if strings.Contains(c.Message, "Merge pull request") && len(messageFields) > 6 {
				message = strings.Join(messageFields[6:], " ")
			}
			if len(strings.Fields(message)) < 4 {
				return nil
			}
			versionInfo.Commits = append(versionInfo.Commits, CommitInfo{Message: message, Hash: c.Hash.String()})
			return nil
		})
		if err != nil {
			log.Fatal(err)
		}

		data.Versions = append(data.Versions, versionInfo)

		if !strings.HasPrefix(nextTag.Name().Short(), "v2.") {
			break
		}
	}

	// remove any versions that have no commits
	var newVersions []VersionInfo
	for _, version := range data.Versions {
		if len(version.Commits) > 0 {
			newVersions = append(newVersions, version)
		}
	}
	data.Versions = newVersions

	tmpl := `# Version history

{{- range .Versions }}

### [{{ .Version }}](https://github.com/schollz/_core/releases/tag/{{ .Version }}) - {{ .Timestamp.Format "January 2, 2006"}}
{{- range .Commits }}
- {{ .Message }} [<i class="fas fa-code-branch"></i>](https://github.com/schollz/_core/commit/{{.Hash}})
{{- end }}
{{- end }}`

	t, err := template.New("versions").Parse(tmpl)
	if err != nil {
		log.Fatal(err)
	}

	folder, _ := path.Split(os.Args[1])
	os.MkdirAll(folder, 0777)
	file, err := os.Create(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}
	defer file.Close()

	err = t.Execute(file, data)
	if err != nil {
		log.Fatal(err)
	}
}
