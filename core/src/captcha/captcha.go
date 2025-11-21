package captcha

import (
	"net/http"
	"strings"

	"github.com/dchest/captcha"
)

func Generate() string {
	return captcha.New()
}

func Verify(id, digits string) bool {
	return captcha.VerifyString(id, digits)
}

func Serve(w http.ResponseWriter, r *http.Request, id string) {
	id = strings.TrimSuffix(id, ".png")
	if id == "" {
		http.NotFound(w, r)
		return
	}
	if r.URL.Query().Get("reload") != "" {
		captcha.Reload(id)
	}
	w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate")
	w.Header().Set("Pragma", "no-cache")
	w.Header().Set("Expires", "0")
	captcha.WriteImage(w, id, captcha.StdWidth, captcha.StdHeight)
}
