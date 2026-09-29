package server

// Product selects the website presentation, independently of connected hardware.
type Product string

const (
	ProductZeptocore  Product = "zeptocore"
	ProductEzeptocore Product = "ezeptocore"
	ProductEctocore   Product = "ectocore"
)

func (p Product) Name() string {
	switch p {
	case ProductEctocore:
		return "Ectocore"
	case ProductEzeptocore:
		return "Ezeptocore"
	default:
		return "Zeptocore"
	}
}
