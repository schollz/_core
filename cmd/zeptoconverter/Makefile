zeptoconverter:
	go build -v

clean:
	rm -rf test2 
	rm -rf zeptoconverter

upload:
	rm -rf test2 
	go run main.go -in test -out test2
	rm -rf /media/zns/zeptocore/bank0 /media/zns/zeptocore/bank1
	rsync -avrP test2/ /media/zns/zeptocore/
	umount /media/zns/zeptocore

readspeed:
	sudo hdparm -t /dev/sdd