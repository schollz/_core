/* hw_config.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/
/*

This file should be tailored to match the hardware design.

There should be one element of the spi[] array for each hardware SPI used.

There should be one element of the sd_cards[] array for each SD card slot.
The name is should correspond to the FatFs "logical drive" identifier.
(See http://elm-chan.org/fsw/ff/doc/filename.html#vol)
The rest of the constants will depend on the type of
socket, which SPI it is driven by, and how it is wired.

*/

#include <string.h>
//
#include "my_debug.h"
//
#include "hw_config.h"
//
#include "ff.h" /* Obtains integer types */
//
#include "diskio.h" /* Declarations of disk functions */

// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects.
static spi_t spis[] = {
    // One for each SPI.
};
/* SDIO Interface */
static sd_sdio_if_t sdio_if = {
    .CMD_gpio = SDCARD_CMD_GPIO,
    .D0_gpio = SDCARD_D0_GPIO,
    .set_drive_strength = true,
    .CLK_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .CMD_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .D0_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .D1_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .D2_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .D3_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .SDIO_PIO = pio1,
    .DMA_IRQ_num = DMA_IRQ_0,
#if PICO_RP2040
    // The default system clock frequency for SDK is 125MHz.
    .baud_rate = 125 * 1000 * 1000 / 4  // 31250000 Hz
#endif
#if PICO_RP2350
    //◦The default system clock on RP2350 is 150Mhz.
    .baud_rate = 150 * 1000 * 1000 / 6  // 25000000 Hz, clk_div = 1.5
#endif
};

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = {  // One for each SD card
    {
        .pcName = "0:",  // Name used to mount device
        .type = SD_IF_SDIO,
        .sdio_if_p = &sdio_if, 
        // SD Card detect:
        .use_card_detect = SDCARD_USE_CD,
        .card_detect_gpio = SDCARD_CD_GPIO,  // Card detect
        .card_detected_true = 1  // What the GPIO read returns when a card is
                                 // present.
    }};

/* ********************************************************************** */
size_t sd_get_num() { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num) {
  if (num <= sd_get_num()) {
    return &sd_cards[num];
  } else {
    return NULL;
  }
}
size_t spi_get_num() { return count_of(spis); }
spi_t *spi_get_by_num(size_t num) {
  if (num <= spi_get_num()) {
    return &spis[num];
  } else {
    return NULL;
  }
}

/* [] END OF FILE */
