package email

import (
	"os"

	"github.com/mailjet/mailjet-apiv3-go/v3"
	log "github.com/schollz/logger"
)

var mailjetClient *mailjet.Client

func Init() {
	apiKey := os.Getenv("MAILJET_API_KEY")
	apiSecret := os.Getenv("MAILJET_API_SECRET")
	if apiKey == "" || apiSecret == "" {
		log.Warn("MAILJET_API_KEY or MAILJET_API_SECRET not set. Email sending will be disabled.")
		return
	}
	mailjetClient = mailjet.NewMailjetClient(apiKey, apiSecret)
}

func SendEmail(toEmail, toName, subject, htmlBody string) error {
	if mailjetClient == nil {
		log.Warn("Mailjet client not initialized. Cannot send email.")
		return nil // Or return an error if email is critical
	}

	messagesInfo := []mailjet.InfoMessagesV31{
		{
			From: &mailjet.RecipientV31{
				Email: "no-reply@infinitedigits.co",
				Name:  "Infinite Digits",
			},
			To: &mailjet.RecipientsV31{
				mailjet.RecipientV31{
					Email: toEmail,
					Name:  toName,
				},
			},
			Subject:  subject,
			HTMLPart: htmlBody,
		},
	}

	messages := mailjet.MessagesV31{Info: messagesInfo}
	_, err := mailjetClient.SendMailV31(&messages)
	return err
}
