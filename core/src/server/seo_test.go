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

func renderStaticIndexForHost(t *testing.T, isZeptocore bool, host string) string {
	t.Helper()

	b, err := staticFiles.ReadFile("static/index.html")
	if err != nil {
		t.Fatal(err)
	}
	tmpl, err := template.New("index").Delims("[[", "]]").Parse(string(b))
	if err != nil {
		t.Fatal(err)
	}

	purchase := purchaseDestinationForHost(host, isZeptocore)
	var rendered bytes.Buffer
	err = tmpl.Execute(&rendered, map[string]any{
		"IsZeptocore":    isZeptocore,
		"IsEctocore":     !isZeptocore,
		"BuyURL":         purchase.URL,
		"BuyProductName": purchase.ProductName,
	})
	if err != nil {
		t.Fatal(err)
	}
	return rendered.String()
}

func renderStaticIndex(t *testing.T, isZeptocore bool) string {
	t.Helper()
	return renderStaticIndexForHost(t, isZeptocore, "localhost")
}

func TestPurchaseDestinationForHost(t *testing.T) {
	tests := []struct {
		name        string
		host        string
		isZeptocore bool
		wantProduct string
		wantURL     string
	}{
		{
			name:        "Zeptocore domain",
			host:        "zeptocore.com",
			wantProduct: "Zeptocore",
			wantURL:     "https://shop.infinitedigits.co/collections/zeptocore/",
		},
		{
			name:        "Zeptocore subdomain with port",
			host:        "tool.zeptocore.com:443",
			wantProduct: "Zeptocore",
			wantURL:     "https://shop.infinitedigits.co/collections/zeptocore/",
		},
		{
			name:        "Ezeptocore domain with port",
			host:        "ezeptocore.com:443",
			wantProduct: "Ezeptocore",
			wantURL:     "https://shop.infinitedigits.co/collections/ezeptocore/",
		},
		{
			name:        "Ectocore domain",
			host:        "ectocore.rocks",
			wantProduct: "Ectocore",
			wantURL:     "https://shop.infinitedigits.co/collections/ectocore/",
		},
		{
			name:        "Local Zeptocore default",
			host:        "localhost:8101",
			isZeptocore: true,
			wantProduct: "Zeptocore",
			wantURL:     "https://shop.infinitedigits.co/collections/zeptocore/",
		},
		{
			name:        "Local Eurorack default",
			host:        "localhost:8100",
			wantProduct: "Ezeptocore",
			wantURL:     "https://shop.infinitedigits.co/collections/ezeptocore/",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got := purchaseDestinationForHost(test.host, test.isZeptocore)
			if got.ProductName != test.wantProduct {
				t.Errorf("product = %q, want %q", got.ProductName, test.wantProduct)
			}
			if got.URL != test.wantURL {
				t.Errorf("URL = %q, want %q", got.URL, test.wantURL)
			}
		})
	}
}

func TestBuyLinkByHost(t *testing.T) {
	buyLinkPattern := regexp.MustCompile(`(?s)<a[^>]*id="buyLink"[^>]*>.*?</a>`)
	tests := []struct {
		name        string
		host        string
		isZeptocore bool
		product     string
		url         string
	}{
		{
			name:        "Zeptocore",
			host:        "zeptocore.com",
			isZeptocore: true,
			product:     "Zeptocore",
			url:         "https://shop.infinitedigits.co/collections/zeptocore/",
		},
		{
			name:    "Ezeptocore",
			host:    "ezeptocore.com",
			product: "Ezeptocore",
			url:     "https://shop.infinitedigits.co/collections/ezeptocore/",
		},
		{
			name:    "Ectocore",
			host:    "ectocore.rocks",
			product: "Ectocore",
			url:     "https://shop.infinitedigits.co/collections/ectocore/",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			html := renderStaticIndexForHost(t, test.isZeptocore, test.host)
			buyLinks := buyLinkPattern.FindAllString(html, -1)
			if len(buyLinks) != 1 {
				t.Fatalf("rendered tool contains %d buy links, want 1", len(buyLinks))
			}
			buyLink := buyLinks[0]
			for _, expected := range []string{
				`href="` + test.url + `"`,
				`title="Buy ` + test.product + `"`,
				`aria-label="Buy ` + test.product + `"`,
				`target="_blank"`,
				`rel="noopener noreferrer"`,
				`class="fa-solid fa-shopping-cart"`,
			} {
				if !strings.Contains(buyLink, expected) {
					t.Errorf("rendered %s tool is missing %q", test.product, expected)
				}
			}
		})
	}
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
