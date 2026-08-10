package server

import (
	"bytes"
	"encoding/json"
	"html/template"
	"net/http/httptest"
	"regexp"
	"strings"
	"testing"
)

func renderStaticIndex(t *testing.T, isZeptocore bool) string {
	t.Helper()

	b, err := staticFiles.ReadFile("static/index.html")
	if err != nil {
		t.Fatal(err)
	}
	tmpl, err := template.New("index").Delims("[[", "]]").Parse(string(b))
	if err != nil {
		t.Fatal(err)
	}

	var rendered bytes.Buffer
	err = tmpl.Execute(&rendered, map[string]bool{
		"IsZeptocore": isZeptocore,
		"IsEctocore":  !isZeptocore,
	})
	if err != nil {
		t.Fatal(err)
	}
	return rendered.String()
}

func TestZeptocoreToolSEO(t *testing.T) {
	html := renderStaticIndex(t, true)
	for _, expected := range []string{
		"<title>Zeptocore Sample Tool",
		`name="description"`,
		`rel="canonical" href="https://zeptocore.com/tool"`,
		`property="og:title"`,
		`name="twitter:card"`,
		`href="/static/manifest.json"`,
	} {
		if !strings.Contains(html, expected) {
			t.Errorf("rendered Zeptocore tool is missing %q", expected)
		}
	}

	match := regexp.MustCompile(`(?s)<script type="application/ld\+json">\s*(\{.*?\})\s*</script>`).FindStringSubmatch(html)
	if len(match) != 2 || !json.Valid([]byte(match[1])) {
		t.Error("rendered Zeptocore tool has invalid JSON-LD")
	}
}

func TestEzeptocoreToolDoesNotUseZeptocoreCanonical(t *testing.T) {
	html := renderStaticIndex(t, false)
	if !strings.Contains(html, `rel="canonical" href="https://get.ezeptocore.com/"`) {
		t.Error("rendered Ezeptocore tool is missing its canonical URL")
	}
	if !strings.Contains(html, `href="/static/ecto/manifest.json"`) {
		t.Error("rendered Ezeptocore tool is missing its own manifest")
	}
	if strings.Contains(html, `rel="canonical" href="https://zeptocore.com/tool"`) {
		t.Error("rendered Ezeptocore tool contains the Zeptocore canonical URL")
	}
}

func TestRobotsAdvertisesCanonicalSitemap(t *testing.T) {
	request := httptest.NewRequest("GET", "/robots.txt", nil)
	response := httptest.NewRecorder()
	if err := handle(response, request); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(response.Body.String(), "Sitemap: https://zeptocore.com/sitemap.xml") {
		t.Error("robots.txt does not advertise the canonical sitemap")
	}
}
