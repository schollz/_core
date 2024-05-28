package main

import (
	"fmt"
	"log"
	"strings"

	git "github.com/go-git/go-git/v5"
	"github.com/go-git/go-git/v5/plumbing/object"
)

func main() {
	// r, err := git.Clone(memory.NewStorage(), nil, &git.CloneOptions{
	// 	URL: "https://github.com/schollz/_core",
	// })
	// Open the current repository
	r, err := git.PlainOpen("../..")
	if err != nil {
		log.Fatal(err)
	}
	if err != nil {
		log.Fatal(err)
	}

	ref, err := r.Head()
	if err != nil {
		log.Fatal(err)
	}

	// ... retrieves the commit history
	cIter, err := r.Log(&git.LogOptions{From: ref.Hash()})
	if err != nil {
		log.Fatal(err)
	}

	// ... just iterates over the commits, printing it
	err = cIter.ForEach(func(c *object.Commit) error {
		message := c.Message
		if strings.Contains(message, "zeptocore:") {
			features := []string{}
			// go through line by line and add any line that has "zeptocore:"
			for _, line := range strings.Split(message, "\n") {
				if strings.Contains(line, "zeptocore:") {
					features = append(features, strings.TrimSpace(strings.Split(line, "zeptocore:")[1]))
				}
			}
			fmt.Printf("%s: %s\n", c.Author.When.Format("2006-01-02"), strings.Join(features, ", "))
		}
		return nil
	})
	if err != nil {
		log.Fatal(err)
	}
}
