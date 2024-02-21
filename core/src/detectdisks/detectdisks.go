package detectdisks

import (
	"fmt"

	log "github.com/schollz/logger"
	"github.com/shirou/gopsutil/disk"
)

func GetUF2Drive() (uf2disk string, err error) {
	partitions, err := disk.Partitions(true) // 'true' to include all devices
	if err != nil {
		log.Error(err)
		return
	}

	for _, partition := range partitions {

		// Optional: Get usage statistics for each partition
		usageStat, err := disk.Usage(partition.Mountpoint)
		if err != nil {
			continue
		}

		if usageStat.Used == 8192 || usageStat.Used == 20480 {
			uf2disk = partition.Mountpoint
			log.Debug(partition.Mountpoint, " ", usageStat.Total, " ", usageStat.Free, " ", usageStat.Used)
		} else {
			log.Trace(partition.Mountpoint, " ", usageStat.Total, " ", usageStat.Free, " ", usageStat.Used)
		}
	}
	if uf2disk == "" {
		err = fmt.Errorf("no UF2 drive found")
	}
	return
}
