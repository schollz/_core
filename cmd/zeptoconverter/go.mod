module github.com/schollz/zeptocore/cmd/zeptoconverter

go 1.21.3

require (
	github.com/schollz/logger v1.2.0
	github.com/schollz/progressbar/v3 v3.14.1
)

require (
	github.com/mattn/go-runewidth v0.0.15 // indirect
	github.com/mitchellh/colorstring v0.0.0-20190213212951-d06e56a500db // indirect
	github.com/rivo/uniseg v0.4.4 // indirect
	golang.org/x/sys v0.14.0 // indirect
	golang.org/x/term v0.14.0 // indirect
)

replace github.com/schollz/zeptocore/cmd/zeptoserver/src/server => src/server

replace github.com/schollz/zeptocore/cmd/zeptoserver/src/renoise => src/renoise

replace github.com/schollz/zeptocore/cmd/zeptoserver/src/op1 => src/renoise

replace github.com/schollz/zeptocore/cmd/zeptoserver/src/sox => src/sox
