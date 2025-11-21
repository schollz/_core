package auth

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"path"
	"sync"
	"time"

	log "github.com/schollz/logger"
	"golang.org/x/crypto/bcrypt"
)

type User struct {
	ID                      string `json:"id"`
	Username                string `json:"username"`
	Email                   string `json:"email"`
	PasswordHash            string `json:"password_hash"`
	EmailVerified           bool   `json:"email_verified"`
	VerificationToken       string `json:"verification_token,omitempty"`
	VerificationTokenExpiry int64  `json:"verification_token_expiry,omitempty"`
	MagicToken              string `json:"magic_token,omitempty"`
	MagicTokenExpiry        int64  `json:"magic_token_expiry,omitempty"`
	CreatedAt               int64  `json:"created_at"`
}

var usersFile = "storage/users.json"
var usersMutex sync.RWMutex
var users map[string]*User

func InitUsers() error {
	users = make(map[string]*User)

	os.MkdirAll("storage", 0777)

	data, err := os.ReadFile(usersFile)
	if err != nil {
		if os.IsNotExist(err) {
			return saveUsers()
		}
		return err
	}

	if err := json.Unmarshal(data, &users); err != nil {
		return err
	}

	log.Debugf("loaded %d users", len(users))
	return nil
}

func saveUsers() error {
	data, err := json.MarshalIndent(users, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(usersFile, data, 0600)
}

func HashPassword(password string) (string, error) {
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), 14)
	return string(bytes), err
}

func GenerateUserID() (string, error) {
	b := make([]byte, 16)
	_, err := rand.Read(b)
	if err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}

func CreateUser(email, password string) (*User, error) {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	for _, u := range users {
		if u.Email == email {
			return nil, fmt.Errorf("email already exists")
		}
	}

	userID, err := GenerateUserID()
	if err != nil {
		return nil, err
	}

	passwordHash, err := HashPassword(password)
	if err != nil {
		return nil, err
	}

	user := &User{
		ID:            userID,
		Username:      email,
		Email:         email,
		PasswordHash:  passwordHash,
		EmailVerified: false,
		CreatedAt:     time.Now().Unix(),
	}

	users[userID] = user

	if err := saveUsers(); err != nil {
		return nil, err
	}

	log.Debugf("created user: %s", email)
	return user, nil
}

func AuthenticateUser(username, password string) (*User, error) {
	usersMutex.RLock()
	defer usersMutex.RUnlock()

	for _, u := range users {
		if u.Email == username {
			err := bcrypt.CompareHashAndPassword([]byte(u.PasswordHash), []byte(password))
			if err == nil {
				if !u.EmailVerified {
					return nil, fmt.Errorf("email not verified")
				}
				return u, nil
			}
		}
	}

	return nil, fmt.Errorf("invalid email or password")
}

func GetUser(userID string) (*User, error) {
	usersMutex.RLock()
	defer usersMutex.RUnlock()

	user, ok := users[userID]
	if !ok {
		return nil, fmt.Errorf("user not found")
	}

	return user, nil
}

func GetUserByEmail(email string) (*User, error) {
	usersMutex.RLock()
	defer usersMutex.RUnlock()

	for _, u := range users {
		if u.Email == email {
			return u, nil
		}
	}

	return nil, fmt.Errorf("user not found")
}

func SetVerificationToken(userID string) (*User, error) {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	user, ok := users[userID]
	if !ok {
		return nil, fmt.Errorf("user not found")
	}

	token, err := GenerateUserID()
	if err != nil {
		return nil, err
	}

	user.VerificationToken = token
	user.VerificationTokenExpiry = time.Now().Add(24 * time.Hour).Unix()

	if err := saveUsers(); err != nil {
		return nil, err
	}

	return user, nil
}

func VerifyEmail(token string) (*User, error) {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	for _, u := range users {
		if u.VerificationToken == token && u.VerificationTokenExpiry > time.Now().Unix() {
			u.EmailVerified = true
			u.VerificationToken = ""
			u.VerificationTokenExpiry = 0
			if err := saveUsers(); err != nil {
				return nil, err
			}
			return u, nil
		}
	}

	return nil, fmt.Errorf("invalid or expired verification token")
}

func SetMagicToken(email string) (*User, error) {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	var user *User
	for _, u := range users {
		if u.Email == email {
			user = u
			break
		}
	}

	if user == nil {
		return nil, fmt.Errorf("user not found")
	}

	if !user.EmailVerified {
		return nil, fmt.Errorf("email not verified")
	}

	token, err := GenerateUserID()
	if err != nil {
		return nil, err
	}

	user.MagicToken = token
	user.MagicTokenExpiry = time.Now().Add(15 * time.Minute).Unix()

	if err := saveUsers(); err != nil {
		return nil, err
	}

	return user, nil
}

func GetUserByMagicToken(token string) (*User, error) {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	for _, u := range users {
		if u.MagicToken == token && u.MagicTokenExpiry > time.Now().Unix() {
			u.MagicToken = ""
			u.MagicTokenExpiry = 0
			if err := saveUsers(); err != nil {
				return nil, err
			}
			return u, nil
		}
	}

	return nil, fmt.Errorf("invalid or expired magic token")
}

func ChangePassword(userID, newPassword string) error {
	usersMutex.Lock()
	defer usersMutex.Unlock()

	user, ok := users[userID]
	if !ok {
		return fmt.Errorf("user not found")
	}

	passwordHash, err := HashPassword(newPassword)
	if err != nil {
		return err
	}

	user.PasswordHash = passwordHash

	return saveUsers()
}

func GetUserStorageFolder(userID string) string {
	return path.Join("storage", "users", userID)
}
