package main

import (
	"fmt"

	"github.com/mpiannucci/peakdetect"
)

func main() {
	v := [...]float64{4268.583874124999, 18703.76432725, 17952.061206000002, 12376.92988, 3053.0212921875, 16373.03294275, 15891.386848812497, 11836.223193000002, 3066.7199090000004, 16300.080047, 33319.345180062504, 10851.986135125, 2942.7398975625006, 17740.363762687502, 11843.484028187499, 26794.848054749997}
	for _, val := range v {
		fmt.Println(val)
	}
	_, _, maxi, _ := peakdetect.PeakDetect(v[:], 1.0)
	fmt.Println("peaks")
	for _, v := range maxi {
		fmt.Println(v)
	}
}
