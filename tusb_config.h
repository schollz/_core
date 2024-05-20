#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST

#define CFG_TUH_HUB                 1
#define CFG_TUH_CDC                 0
#define CFG_TUH_HID                 4
#define CFG_TUH_MSC                 0
#define CFG_TUH_VENDOR              0

#define CFG_TUSB_HOST_DEVICE_MAX    (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports

