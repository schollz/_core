module github.com/schollz/zeptocore/cmd/zeptoconverter

go 1.21.3

require (
	github.com/schollz/logger v1.2.0
	github.com/schollz/progressbar/v3 v3.13.1
	github.com/schollz/zeptoconverter/lib/sox v0.0.0-20231003145150-6ceb4181fcba
)

require (
	github.com/mattn/go-runewidth v0.0.14 // indirect
	github.com/mitchellh/colorstring v0.0.0-20190213212951-d06e56a500db // indirect
	github.com/rivo/uniseg v0.2.0 // indirect
	github.com/schollz/zeptoconverter/lib/op1 v0.0.0-00010101000000-000000000000 // indirect
	github.com/schollz/zeptoconverter/lib/renoise v0.0.0-00010101000000-000000000000 // indirect
	golang.org/x/sys v0.6.0 // indirect
	golang.org/x/term v0.6.0 // indirect
)

replace github.com/schollz/zeptoconverter/lib/renoise => ../../lib/renoise

replace github.com/schollz/zeptoconverter/lib/op1 => ../../lib/op1

replace github.com/schollz/zeptoconverter/lib/sox => ../../lib/sox
