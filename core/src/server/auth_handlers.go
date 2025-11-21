package server

import (
	"encoding/json"
	"html/template"
	"net/http"
	"os"
	"path"
	"strings"

	"github.com/schollz/_core/core/src/auth"
	"github.com/schollz/_core/core/src/captcha"
	"github.com/schollz/_core/core/src/email"
	"github.com/schollz/codename"
	log "github.com/schollz/logger"
)

func handleLogin(w http.ResponseWriter, r *http.Request) error {
	if r.Method == "GET" {
		return renderLoginPage(w, r, "")
	}

	if r.Method == "POST" {
		emailAddr := strings.TrimSpace(r.FormValue("email"))
		password := r.FormValue("password")

		if emailAddr == "" {
			return renderLoginPage(w, r, "Email is required")
		}

		if password == "" {
			return renderLoginPage(w, r, "Enter your password or request a magic link instead.")
		}

		user, err := auth.AuthenticateUser(emailAddr, password)
		if err != nil {
			log.Debugf("password login failed for %s: %s", emailAddr, err)
			if strings.Contains(err.Error(), "verified") {
				return renderLoginPage(w, r, "Please verify your email before logging in.")
			}
			return renderLoginPage(w, r, "Invalid email or password.")
		}

		token, err := auth.CreateToken(user.ID, user.Username)
		if err != nil {
			log.Error(err)
			return renderLoginPage(w, r, "Failed to create session")
		}

		auth.SetAuthCookie(w, token)
		log.Debugf("user %s logged in", emailAddr)
		http.Redirect(w, r, "/dashboard", http.StatusSeeOther)
		return nil
	}

	http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
	return nil
}

func handleRegister(w http.ResponseWriter, r *http.Request) error {
	if r.Method == "GET" {
		captchaId := captcha.Generate()
		return renderRegisterPage(w, r, "", captchaId)
	}

	if r.Method == "POST" {
		emailAddr := strings.TrimSpace(r.FormValue("email"))
		password := r.FormValue("password")
		confirmPassword := r.FormValue("confirm_password")
		captchaId := strings.TrimSpace(r.FormValue("captcha_id"))
		captchaSolution := strings.TrimSpace(r.FormValue("captcha_solution"))

		if !captcha.Verify(captchaId, captchaSolution) {
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, "Invalid captcha", captchaId)
		}

		if emailAddr == "" {
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, "Email is required", captchaId)
		}

		if len(password) < 6 {
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, "Password must be at least 6 characters", captchaId)
		}

		if password != confirmPassword {
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, "Passwords do not match", captchaId)
		}

		user, err := auth.CreateUser(emailAddr, password)
		if err != nil {
			log.Debugf("registration failed: %s", err)
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, err.Error(), captchaId)
		}

		// Send verification email
		user, err = auth.SetVerificationToken(user.ID)
		if err != nil {
			log.Error(err)
			captchaId = captcha.Generate()
			return renderRegisterPage(w, r, "Failed to send verification email", captchaId)
		}
		verificationLink := "http://" + r.Host + "/verify?token=" + user.VerificationToken
		go email.SendEmail(user.Email, user.Username, "Verify your email", "Hello "+user.Username+",<br><br>Please click the link below to verify your email address:<br><a href=\""+verificationLink+"\">"+verificationLink+"</a>")

		http.Redirect(w, r, "/verify-email", http.StatusSeeOther)
		return nil
	}

	http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
	return nil
}

func handleLogout(w http.ResponseWriter, r *http.Request) error {
	auth.ClearAuthCookie(w)
	http.Redirect(w, r, "/login", http.StatusSeeOther)
	return nil
}

func handleDashboard(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Redirect(w, r, "/login", http.StatusSeeOther)
		return nil
	}

	return renderDashboard(w, r, claims)
}

func handleCheckAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"authenticated": false,
		})
		return nil
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"authenticated": true,
		"username":      claims.Username,
		"user_id":       claims.UserID,
	})
	return nil
}

func renderLoginPage(w http.ResponseWriter, r *http.Request, errorMsg string) error {
	tmpl := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]] - Login</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: [[ if .IsEctocore ]]#f0f0f0[[ else ]]#f4f0ff[[ end ]];
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        .header {
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#bdafeb[[ end ]];
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header h1 {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-size: 1.5rem;
            font-weight: 600;
        }

        .container {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 2rem;
        }

        .login-box {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
            width: 100%;
            max-width: 400px;
        }

        .login-box h2 {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 1.5rem;
            font-size: 1.5rem;
        }

        .form-group {
            margin-bottom: 1rem;
        }

        .form-group label {
            display: block;
            margin-bottom: 0.5rem;
            color: #333;
            font-weight: 500;
        }

        .form-group input {
            width: 100%;
            padding: 0.75rem;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 1rem;
        }

        .form-group input:focus {
            outline: none;
            border-color: [[ if .IsEctocore ]]#d4af37[[ else ]]#9b82db[[ end ]];
        }

        .btn {
            width: 100%;
            padding: 0.75rem;
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 1rem;
            cursor: pointer;
            font-weight: 600;
        }

        .btn:hover {
            background: [[ if .IsEctocore ]]#333[[ else ]]#8a71ca[[ end ]];
        }

        .error {
            background: #fee;
            border: 1px solid #fcc;
            color: #c33;
            padding: 0.75rem;
            border-radius: 4px;
            margin-bottom: 1rem;
        }

        .link {
            text-align: center;
            margin-top: 1rem;
            color: #666;
        }

        .link a {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            text-decoration: none;
            font-weight: 600;
        }

        .link a:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]]</h1>
    </div>
    <div class="container">
        <div class="login-box">
            <h2>Login</h2>
            [[ if .Message ]]
            <div class="error">[[ .Message ]]</div>
            [[ end ]]
            <form method="POST" action="/login">
                <div class="form-group">
                    <label for="email">Email</label>
                    <input type="email" id="email" name="email" required autocomplete="email">
                </div>
                <div class="form-group">
                    <label for="password">Password</label>
                    <input type="password" id="password" name="password" required autocomplete="current-password">
                </div>
                <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
                    <button type="submit" class="btn">Login</button>
                </div>
            </form>
            <div class="link">
                Prefer a link? <a href="/magic-link">Send a magic link</a>
            </div>
            <div class="link">
                Don't have an account? <a href="/register">Register</a>
            </div>
        </div>
    </div>
</body>
</html>`

	t, err := template.New("login").Delims("[[", "]]").Parse(tmpl)
	if err != nil {
		return err
	}

	data := struct {
		IsEctocore bool
		Message    string
	}{
		IsEctocore: isEctocore,
		Message:    errorMsg,
	}

	return t.Execute(w, data)
}

func renderRegisterPage(w http.ResponseWriter, r *http.Request, errorMsg, captchaId string) error {
	tmpl := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]] - Register</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: [[ if .IsEctocore ]]#f0f0f0[[ else ]]#f4f0ff[[ end ]];
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        .header {
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#bdafeb[[ end ]];
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header h1 {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-size: 1.5rem;
            font-weight: 600;
        }

        .container {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 2rem;
        }

        .register-box {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
            width: 100%;
            max-width: 400px;
        }

        .register-box h2 {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 1.5rem;
            font-size: 1.5rem;
        }

        .form-group {
            margin-bottom: 1rem;
        }

        .form-group label {
            display: block;
            margin-bottom: 0.5rem;
            color: #333;
            font-weight: 500;
        }

        .form-group input {
            width: 100%;
            padding: 0.75rem;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 1rem;
        }

        .form-group input:focus {
            outline: none;
            border-color: [[ if .IsEctocore ]]#d4af37[[ else ]]#9b82db[[ end ]];
        }
        
        .captcha-img {
            cursor: pointer;
        }

        .btn {
            width: 100%;
            padding: 0.75rem;
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 1rem;
            cursor: pointer;
            font-weight: 600;
        }

        .btn:hover {
            background: [[ if .IsEctocore ]]#333[[ else ]]#8a71ca[[ end ]];
        }

        .error {
            background: #fee;
            border: 1px solid #fcc;
            color: #c33;
            padding: 0.75rem;
            border-radius: 4px;
            margin-bottom: 1rem;
        }

        .link {
            text-align: center;
            margin-top: 1rem;
            color: #666;
        }

        .link a {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            text-decoration: none;
            font-weight: 600;
        }

        .link a:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]]</h1>
    </div>
    <div class="container">
        <div class="register-box">
            <h2>Register</h2>
            [[ if .Error ]]
            <div class="error">[[ .Error ]]</div>
            [[ end ]]
            <form method="POST" action="/register">
                <div class="form-group">
                    <label for="email">Email</label>
                    <input type="email" id="email" name="email" required autocomplete="email">
                </div>
                <div class="form-group">
                    <label for="password">Password</label>
                    <input type="password" id="password" name="password" required autocomplete="new-password" minlength="6">
                </div>
                <div class="form-group">
                    <label for="confirm_password">Confirm Password</label>
                    <input type="password" id="confirm_password" name="confirm_password" required autocomplete="new-password" minlength="6">
                </div>
                <div class="form-group">
                    <label for="captcha_solution">Captcha</label>
                    <img src="/captcha/[[ .CaptchaId ]].png" alt="Captcha Image" class="captcha-img" onclick="this.src='/captcha/[[ .CaptchaId ]].png?reload=1&ts=' + (new Date()).getTime()">
                    <input type="text" id="captcha_solution" name="captcha_solution" required autocomplete="off">
                    <input type="hidden" name="captcha_id" value="[[ .CaptchaId ]]">
                </div>
                <button type="submit" class="btn">Register</button>
            </form>
            <div class="link">
                Already have an account? <a href="/login">Login</a>
            </div>
        </div>
    </div>
</body>
</html>`

	t, err := template.New("register").Delims("[[", "]]").Parse(tmpl)
	if err != nil {
		return err
	}

	data := struct {
		IsEctocore bool
		Error      string
		CaptchaId  string
	}{
		IsEctocore: isEctocore,
		Error:      errorMsg,
		CaptchaId:  captchaId,
	}

	return t.Execute(w, data)
}

func handleVerifyEmail(w http.ResponseWriter, r *http.Request) error {
	token := r.URL.Query().Get("token")
	if token == "" {
		return renderVerifyEmailPage(w, r, "Check your email for a verification link.")
	}

	user, err := auth.VerifyEmail(token)
	if err != nil {
		return renderVerifyEmailPage(w, r, "Invalid or expired verification link.")
	}

	log.Debugf("email verified for %s", user.Email)
	return renderVerifyEmailPage(w, r, "Email successfully verified. You can now <a href=\"/login\">login</a>.")
}

func handleMagicLink(w http.ResponseWriter, r *http.Request) error {
	if r.Method == "GET" {
		captchaId := captcha.Generate()
		return renderMagicLinkPage(w, r, "", captchaId)
	}

	if r.Method == "POST" {
		emailAddr := strings.TrimSpace(r.FormValue("email"))
		captchaId := strings.TrimSpace(r.FormValue("captcha_id"))
		captchaSolution := strings.TrimSpace(r.FormValue("captcha_solution"))

		if !captcha.Verify(captchaId, captchaSolution) {
			captchaId = captcha.Generate()
			return renderMagicLinkPage(w, r, "Invalid captcha", captchaId)
		}

		if emailAddr == "" {
			captchaId = captcha.Generate()
			return renderMagicLinkPage(w, r, "Email is required", captchaId)
		}

		user, err := auth.SetMagicToken(emailAddr)
		if err != nil {
			log.Debugf("magic link failed: %s", err)
			// Don't reveal if user exists or not
			if strings.Contains(err.Error(), "verified") {
				return renderMagicLinkPage(w, r, "Please verify your email before logging in.", captcha.Generate())
			}
			return renderMagicLinkPage(w, r, "If an account with that email exists and is verified, a magic link has been sent.", captcha.Generate())
		}

		magicLink := "http://" + r.Host + "/magic-login?token=" + user.MagicToken
		go email.SendEmail(user.Email, user.Username, "Magic Login Link", "Hello "+user.Username+",<br><br>Click the link below to log in:<br><a href=\""+magicLink+"\">"+magicLink+"</a>")

		return renderMagicLinkPage(w, r, "If an account with that email exists and is verified, a magic link has been sent.", captcha.Generate())
	}

	http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
	return nil
}

func handleMagicLinkLogin(w http.ResponseWriter, r *http.Request) error {
	token := r.URL.Query().Get("token")
	if token == "" {
		return renderLoginPage(w, r, "Invalid magic link.")
	}

	user, err := auth.GetUserByMagicToken(token)
	if err != nil {
		return renderLoginPage(w, r, "Invalid or expired magic link.")
	}

	sessionToken, err := auth.CreateToken(user.ID, user.Username)
	if err != nil {
		log.Error(err)
		return renderLoginPage(w, r, "Failed to create session.")
	}

	auth.SetAuthCookie(w, sessionToken)
	log.Debugf("user %s logged in via magic link", user.Username)

	http.Redirect(w, r, "/dashboard", http.StatusSeeOther)
	return nil
}

func handleChangePassword(w http.ResponseWriter, r *http.Request) error {
	// Placeholder
	return nil
}

func renderVerifyEmailPage(w http.ResponseWriter, r *http.Request, msg string) error {
	tmpl := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]] - Email Verification</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: [[ if .IsEctocore ]]#f0f0f0[[ else ]]#f4f0ff[[ end ]];
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        .header {
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#bdafeb[[ end ]];
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header h1 {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-size: 1.5rem;
            font-weight: 600;
        }

        .container {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 2rem;
            text-align: center;
        }

        .message-box {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
            width: 100%;
            max-width: 400px;
        }

        .message-box p {
            color: #333;
            font-size: 1.1rem;
            line-height: 1.6;
        }

        .message-box a {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            text-decoration: none;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]]</h1>
    </div>
    <div class="container">
        <div class="message-box">
            <p>[[ .Message ]]</p>
        </div>
    </div>
</body>
</html>`

	t, err := template.New("verify-email").Delims("[[", "]]").Parse(tmpl)
	if err != nil {
		return err
	}

	data := struct {
		IsEctocore bool
		Message    template.HTML
	}{
		IsEctocore: isEctocore,
		Message:    template.HTML(msg),
	}

	return t.Execute(w, data)
}

func renderMagicLinkPage(w http.ResponseWriter, r *http.Request, msg string, captchaId string) error {
	tmpl := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]] - Magic Link</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: [[ if .IsEctocore ]]#f0f0f0[[ else ]]#f4f0ff[[ end ]];
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        .header {
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#bdafeb[[ end ]];
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header h1 {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-size: 1.5rem;
            font-weight: 600;
        }

        .container {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 2rem;
        }

        .magic-link-box {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
            width: 100%;
            max-width: 400px;
        }

        .magic-link-box h2 {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 1.5rem;
            font-size: 1.5rem;
        }

        .form-group {
            margin-bottom: 1rem;
        }

        .form-group label {
            display: block;
            margin-bottom: 0.5rem;
            color: #333;
            font-weight: 500;
        }

        .form-group input {
            width: 100%;
            padding: 0.75rem;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 1rem;
        }

        .form-group input:focus {
            outline: none;
            border-color: [[ if .IsEctocore ]]#d4af37[[ else ]]#9b82db[[ end ]];
        }
        
        .captcha-img {
            cursor: pointer;
        }

        .btn {
            width: 100%;
            padding: 0.75rem;
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 1rem;
            cursor: pointer;
            font-weight: 600;
        }

        .btn:hover {
            background: [[ if .IsEctocore ]]#333[[ else ]]#8a71ca[[ end ]];
        }

        .message {
            background: #e6f7ff;
            border: 1px solid #b3e0ff;
            color: #005f99;
            padding: 0.75rem;
            border-radius: 4px;
            margin-bottom: 1rem;
        }

        .link {
            text-align: center;
            margin-top: 1rem;
            color: #666;
        }

        .link a {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            text-decoration: none;
            font-weight: 600;
        }

        .link a:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]]</h1>
    </div>
    <div class="container">
        <div class="magic-link-box">
            <h2>Send Magic Link</h2>
            [[ if .Message ]]
            <div class="message">[[ .Message ]]</div>
            [[ end ]]
            <form method="POST" action="/magic-link">
                <div class="form-group">
                    <label for="email">Email</label>
                    <input type="email" id="email" name="email" required autocomplete="email">
                </div>
                <div class="form-group">
                    <label for="captcha_solution">Captcha</label>
                    <img src="/captcha/[[ .CaptchaId ]].png" alt="Captcha Image" class="captcha-img" onclick="this.src='/captcha/[[ .CaptchaId ]].png?reload=1&ts=' + (new Date()).getTime()">
                    <input type="text" id="captcha_solution" name="captcha_solution" required autocomplete="off">
                    <input type="hidden" name="captcha_id" value="[[ .CaptchaId ]]">
                </div>
                <button type="submit" class="btn">Send Magic Link</button>
            </form>
            <div class="link">
                Remember your password? <a href="/login">Login</a>
            </div>
        </div>
    </div>
</body>
</html>`

	t, err := template.New("magic-link").Delims("[[", "]]").Parse(tmpl)
	if err != nil {
		return err
	}

	data := struct {
		IsEctocore bool
		Message    string
		CaptchaId  string
	}{
		IsEctocore: isEctocore,
		Message:    msg,
		CaptchaId:  captchaId,
	}

	return t.Execute(w, data)
}

func renderChangePasswordPage(w http.ResponseWriter, r *http.Request, errorMsg string) error {
	// Placeholder
	return nil
}

func renderDashboard(w http.ResponseWriter, r *http.Request, claims *auth.Claims) error {
	tmpl := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]] - Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            background: [[ if .IsEctocore ]]#f0f0f0[[ else ]]#f4f0ff[[ end ]];
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }

        .header {
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#bdafeb[[ end ]];
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .header h1 {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-size: 1.5rem;
            font-weight: 600;
        }

        .header-right {
            display: flex;
            align-items: center;
            gap: 1rem;
        }

        .username {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            font-weight: 500;
        }

        .icon-link {
            color: [[ if .IsEctocore ]]#d4af37[[ else ]]#fff[[ end ]];
            text-decoration: none;
            font-size: 1.2rem;
            padding: 0.5rem;
            border-radius: 4px;
            transition: background 0.2s;
        }

        .icon-link:hover {
            background: rgba(255, 255, 255, 0.1);
        }

        .container {
            flex: 1;
            padding: 2rem;
            max-width: 1200px;
            margin: 0 auto;
            width: 100%;
        }

        .welcome {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
            margin-bottom: 2rem;
        }

        .welcome h2 {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 1rem;
        }

        .projects {
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 2rem;
        }

        .projects h3 {
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 1rem;
        }

        .btn {
            padding: 0.75rem 1.5rem;
            background: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 1rem;
            cursor: pointer;
            font-weight: 600;
            text-decoration: none;
            display: inline-block;
        }

        .btn:hover {
            background: [[ if .IsEctocore ]]#333[[ else ]]#8a71ca[[ end ]];
        }

        .project-list {
            margin-top: 1.5rem;
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
            gap: 1rem;
        }

        .project-card {
            background: [[ if .IsEctocore ]]#f9f9f9[[ else ]]#f4f0ff[[ end ]];
            border-radius: 4px;
            padding: 1.5rem;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .project-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.15);
        }

        .project-name {
            font-weight: 600;
            color: [[ if .IsEctocore ]]#1a1a1a[[ else ]]#9b82db[[ end ]];
            margin-bottom: 0.5rem;
        }

        .project-info {
            font-size: 0.875rem;
            color: #666;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>[[ if .IsEctocore ]]ectocore[[ else ]]zeptocore[[ end ]]</h1>
        <div class="header-right">
            <span class="username">[[ .Username ]]</span>
            [[ if not .IsEctocore ]]
            <a href="/docs" class="icon-link" title="Documentation">📚</a>
            [[ end ]]
            <a href="/logout" class="icon-link" title="Logout">🚪</a>
        </div>
    </div>
    <div class="container">
        <div class="welcome">
            <h2>Welcome, [[ .Username ]]!</h2>
            <p>Manage your audio projects and workspaces here.</p>
        </div>
        <div class="projects">
            <h3>Your Projects</h3>
            <a href="/new-project" class="btn">New Project</a>
            <div class="project-list" id="projectList">
                <p>Loading projects...</p>
            </div>
        </div>
    </div>
    <script>
        async function loadProjects() {
            try {
                const response = await fetch('/api/projects');
                const projects = await response.json();
                const projectList = document.getElementById('projectList');

                if (projects.length === 0) {
                    projectList.innerHTML = '<p>No projects yet. Create your first project by visiting a URL like /my-project-name</p>';
                } else {
                    projectList.innerHTML = projects.map(p => {
                        return '<div class="project-card" onclick="location.href=\'/' + p.name + '\'">' +
                            '<div class="project-name">' + p.name + '</div>' +
                            '<div class="project-info">' + (p.fileCount || 0) + ' files</div>' +
                            '</div>';
                    }).join('');
                }
            } catch (err) {
                console.error('Failed to load projects:', err);
                document.getElementById('projectList').innerHTML = '<p>Failed to load projects.</p>';
            }
        }

        loadProjects();
    </script>
</body>
</html>`

	t, err := template.New("dashboard").Delims("[[", "]]").Parse(tmpl)
	if err != nil {
		return err
	}

	data := struct {
		IsEctocore bool
		Username   string
		UserID     string
	}{
		IsEctocore: isEctocore,
		Username:   claims.Username,
		UserID:     claims.UserID,
	}

	return t.Execute(w, data)
}

func handleNewProject(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Redirect(w, r, "/login", http.StatusSeeOther)
		return nil
	}
	_ = claims

	rng, err := codename.DefaultRNG()
	if err != nil {
		return err
	}
	projectName := codename.Generate(rng, 0)

	http.Redirect(w, r, "/"+projectName, http.StatusSeeOther)
	return nil
}

func handleWebsocketAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		query := r.URL.Query()
		token := query.Get("token")
		if token != "" {
			claims, err = auth.ValidateToken(token)
			if err != nil {
				http.Error(w, "Unauthorized", http.StatusUnauthorized)
				return nil
			}
		} else {
			http.Error(w, "Unauthorized", http.StatusUnauthorized)
			return nil
		}
	}

	r.Header.Set("X-User-ID", claims.UserID)
	r.Header.Set("X-Username", claims.Username)
	return handleWebsocket(w, r)
}

func handleUploadAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return nil
	}

	r.Header.Set("X-User-ID", claims.UserID)
	r.Header.Set("X-Username", claims.Username)
	return handleUpload(w, r)
}

func handleDownloadAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return nil
	}

	r.Header.Set("X-User-ID", claims.UserID)
	r.Header.Set("X-Username", claims.Username)
	return handleDownload(w, r)
}

func handleDrumExtractAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return nil
	}

	r.Header.Set("X-User-ID", claims.UserID)
	r.Header.Set("X-Username", claims.Username)
	return handleDrumExtract(w, r)
}

func handleOnsetDetectAuth(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return nil
	}

	r.Header.Set("X-User-ID", claims.UserID)
	r.Header.Set("X-Username", claims.Username)
	return handleOnsetDetect(w, r)
}

func handleGetProjects(w http.ResponseWriter, r *http.Request) error {
	claims, err := auth.GetClaims(r)
	if err != nil {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return nil
	}

	userStorageFolder := auth.GetUserStorageFolder(claims.UserID)
	projects := []map[string]interface{}{}

	entries, err := os.ReadDir(userStorageFolder)
	if err != nil {
		if os.IsNotExist(err) {
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(projects)
			return nil
		}
		return err
	}

	for _, entry := range entries {
		if entry.IsDir() && entry.Name() != "states" {
			projectPath := path.Join(userStorageFolder, entry.Name())
			fileCount := 0

			files, err := os.ReadDir(projectPath)
			if err == nil {
				fileCount = len(files)
			}

			projects = append(projects, map[string]interface{}{
				"name":      entry.Name(),
				"fileCount": fileCount,
			})
		}
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(projects)
	return nil
}
