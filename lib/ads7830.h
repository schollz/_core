
#ifndef LIB_ADS7830_H
#define LIB_ADS7830_H 1

typedef struct ADS7830 {
  uint8_t address;
} ADS7830;

void ADS7830_free(ADS7830 *self) { free(self); }

ADS7830 *ADS7830_malloc(uint8_t address) {
  ADS7830 *self = (ADS7830 *)malloc(sizeof(ADS7830));
  if (self == NULL) {
    perror("Error allocating memory for struct");
    return NULL;
  }
  self->address = address;
  return self;
}

uint8_t ADS7830_read(ADS7830 *self, uint8_t ch) {
  uint8_t commandByte = 0b10000100;
  switch (ch) {
    case 1:
      commandByte = 0b11000100;
      break;
    case 2:
      commandByte = 0b10010100;
      break;
    case 3:
      commandByte = 0b11010100;
      break;
    case 4:
      commandByte = 0b10100100;
      break;
    case 5:
      commandByte = 0b11100100;
      break;
    case 6:
      commandByte = 0b10110100;
      break;
    case 7:
      commandByte = 0b11110100;
      break;
  }
  uint8_t adcValue = -1;

  int bytes_read = i2c_write_timeout_us(i2c_default, self->address,
                                        &commandByte, 1, true, 5000);
  if (bytes_read == 0) {
    return -1;
  }
  i2c_read_timeout_us(i2c_default, self->address, &adcValue, 1, false, 5000);
  // printf("[ads7830] [ch%d] %d\n", ch, adcValue);
  return adcValue;
}

#endif