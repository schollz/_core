package main

import (
	"fmt"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/widget"
)

func main() {
	myApp := app.New()
	myWindow := myApp.NewWindow("ProgressBar Widget")

	progress := widget.NewProgressBar()
	infinite := widget.NewProgressBarInfinite()
	filedialog := dialog.NewFolderOpen(func(f fyne.ListableURI, err error) {
		fmt.Println(f)
		fmt.Println(err)
	}, myWindow)
	filedialog.Show()

	go func() {
		for i := 0.0; i <= 1.0; i += 0.1 {
			time.Sleep(time.Millisecond * 250)
			progress.SetValue(i)
		}
	}()

	myWindow.SetContent(container.NewVBox(progress, infinite))
	myWindow.Resize(fyne.NewSize(800, 600))
	myWindow.ShowAndRun()
}
