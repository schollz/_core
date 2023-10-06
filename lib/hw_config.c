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

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = {  // One for each SD card
    {
        .pcName = "0:",  // Name used to mount device
        .type = SD_IF_SDIO,
        /*
        Pins CLK_gpio, D1_gpio, D2_gpio, and D3_gpio are at offsets from pin
        D0_gpio. The offsets are determined by sd_driver\SDIO\rp2040_sdio.pio.
            CLK_gpio = (D0_gpio + SDIO_CLK_PIN_D0_OFFSET) % 32;
            As of this writing, SDIO_CLK_PIN_D0_OFFSET is 30,
              which is -2 in mod32 arithmetic, so:
            CLK_gpio = D0_gpio -2.
            D1_gpio = D0_gpio + 1;
            D2_gpio = D0_gpio + 2;
            D3_gpio = D0_gpio + 3;
        */
        .sdio_if = {.CMD_gpio = SDCARD_CMD_GPIO,
                    .D0_gpio = SDCARD_D0_GPIO,
                    .SDIO_PIO = pio1,
                    .DMA_IRQ_num = DMA_IRQ_0},
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
