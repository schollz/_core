package server

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

func TestEmbeddedZeptocoreLandingPointsToCanonicalStoreGuide(t *testing.T) {
	b, err := docsFiles.ReadFile("docs/index.html")
	if err != nil {
		t.Fatal(err)
	}
	html := string(b)
	for _, expected := range []string{
		`name=robots content="noindex, follow`,
		`rel=canonical href=https://shop.infinitedigits.co/collections/zeptocore/`,
		`href=https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-guide`,
		`href=https://shop.infinitedigits.co/collections/zeptocore-diy/`,
		`href=https://tool.zeptocore.com/`,
		`href=https://github.com/schollz/_core`,
	} {
		if !strings.Contains(html, expected) {
			t.Errorf("migration landing is missing %q", expected)
		}
	}
	for _, retiredCopy := range []string{"Combo list", "Effect list", "How does the zeptocore compare?"} {
		if strings.Contains(html, retiredCopy) {
			t.Errorf("migration landing still embeds retired manual copy %q", retiredCopy)
		}
	}
}

func TestLegacyDocumentationPathsRedirectToStoreGuide(t *testing.T) {
	previousEctocore := isEctocore
	isEctocore = false
	t.Cleanup(func() { isEctocore = previousEctocore })

	tests := []struct {
		path string
		want string
	}{
		{"/buy", "https://shop.infinitedigits.co/collections/zeptocore/"},
		{"/docs/combos/random-fill", "https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-guide"},
	}
	for _, test := range tests {
		request := httptest.NewRequest(http.MethodGet, test.path, nil)
		response := httptest.NewRecorder()
		if err := handle(response, request); err != nil {
			t.Fatal(err)
		}
		if response.Code != http.StatusPermanentRedirect {
			t.Errorf("%s status = %d, want %d", test.path, response.Code, http.StatusPermanentRedirect)
		}
		if location := response.Header().Get("Location"); location != test.want {
			t.Errorf("%s location = %q, want %q", test.path, location, test.want)
		}
	}
}
