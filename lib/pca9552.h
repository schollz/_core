#define PCA9552_LIB_VERSION (F("0.1.1"))

#define PCA9552_ADDRESS 0x60
#define PCA9552_OUTPUTCOUNT 16

//  REGISTERS
#define PCA9552_INPUT0 0x00
#define PCA9552_INPUT1 0x01
#define PCA9552_PSC0 0x02
#define PCA9552_PWM0 0x03
#define PCA9552_PSC1 0x04
#define PCA9552_PWM1 0x05
#define PCA9552_LS0 0x06
#define PCA9552_LS1 0x07
#define PCA9552_LS2 0x08
#define PCA9552_LS3 0x09

//  MUX OUTPUT MODES
#define PCA9552_MODE_HIGH 0
#define PCA9552_MODE_LOW 1
#define PCA9552_MODE_PWM0 2
#define PCA9552_MODE_PWM1 3

//  ERROR CODES (not all used yet)
#define PCA9552_OK 0x00
#define PCA9552_ERROR 0xFF
#define PCA9552_ERR_WRITE 0xFE
#define PCA9552_ERR_CHAN 0xFD
#define PCA9552_ERR_MODE 0xFC
#define PCA9552_ERR_REG 0xFB
#define PCA9552_ERR_I2C 0xFA

typedef struct PCA9552 {
  uint8_t address;
  uint8_t error;
  uint8_t leds[4][4];
  struct i2c_inst *i2c;
} PCA9552;

uint8_t PCA9552_writeReg(PCA9552 *pca, uint8_t reg, uint8_t value) {
  uint8_t txdata[2] = {reg, value};
  pca->error = i2c_write_blocking(pca->i2c, PCA9552_ADDRESS, txdata, 2, false);
  if (pca->error == 2)
    pca->error = PCA9552_OK;
  else
    pca->error = PCA9552_ERROR;
  return pca->error;
}

uint8_t PCA9552_readReg(PCA9552 *pca, uint8_t reg) {
  uint8_t txdata[1] = {reg};
  int ret;
  ret = i2c_write_blocking(pca->i2c, PCA9552_ADDRESS, txdata, 1, false);
  uint8_t rxdata;
  ret = i2c_read_blocking(pca->i2c, PCA9552_ADDRESS, &rxdata, 1, false);
  return ret;
}

void PCA9552_setPrescaler(PCA9552 *pca, uint8_t gen, uint8_t psc) {
  if (gen == 0)
    PCA9552_writeReg(pca, PCA9552_PSC0, psc);
  else
    PCA9552_writeReg(pca, PCA9552_PSC1, psc);
}

uint8_t PCA9552_getPrescaler(PCA9552 *pca, uint8_t gen) {
  if (gen == 0)
    return PCA9552_readReg(pca, PCA9552_PSC0);
  else
    return PCA9552_readReg(pca, PCA9552_PSC1);
}

void PCA9552_setPWM(PCA9552 *pca, uint8_t gen, uint8_t pwm) {
  if (gen == 0)
    PCA9552_writeReg(pca, PCA9552_PWM0, pwm);
  else
    PCA9552_writeReg(pca, PCA9552_PWM1, pwm);
}

uint8_t PCA9552_getPWM(PCA9552 *pca, uint8_t gen) {
  if (gen == 0)
    return PCA9552_readReg(pca, PCA9552_PWM0);
  else
    return PCA9552_readReg(pca, PCA9552_PWM1);
}

uint8_t PCA9552_setOutputMode(PCA9552 *pca, uint8_t pin, uint8_t mode) {
  if (mode > 3) {
    pca->error = PCA9552_ERR_MODE;
    return pca->error;
  }

  uint8_t reg = PCA9552_LS0;
  while (pin > 3) {
    reg += 1;
    pin -= 4;
  }
  uint8_t ledSelect = PCA9552_readReg(pca, reg);
  ledSelect &= ~(0x03 << (pin * 2));
  ledSelect |= (mode << (pin * 2));

  return PCA9552_writeReg(pca, reg, ledSelect);
}

void PCA9552_pinMode(PCA9552 *pca, uint8_t pin, uint8_t mode) {
  if (mode != 1) PCA9552_setOutputMode(pca, pin, PCA9552_MODE_HIGH);
}

void PCA9552_digitalWrite(PCA9552 *pca, uint8_t pin, uint8_t val) {
  if (val == 0)
    PCA9552_setOutputMode(pca, pin, PCA9552_MODE_LOW);
  else
    PCA9552_setOutputMode(pca, pin, PCA9552_MODE_HIGH);
}

uint8_t PCA9553_ledByte(uint8_t ls[4]) {
  uint8_t byte = 0x00;
  for (uint8_t i = 0; i < 4; i++) {
    if (ls[i] == 0) {
      // led off
      byte |= 0b11 << (6 - 2 * i);
    } else if (ls[i] == 1) {
      // led dim
      byte |= 0b10 << (6 - 2 * i);
    } else if (ls[i] == 2) {
      // led bright
      // do nothing (leave zeros)
    }
  }
  return byte;
}

void PCA9553_ledSet(PCA9552 *pca, uint8_t led, uint8_t state) {
  uint8_t i = led / 4;
  uint8_t j = led % 4;
  pca->leds[i][j] = state;
  PCA9552_writeReg(pca, PCA9552_LS0 + i, PCA9553_ledByte(pca->leds[i]));
}

PCA9552 *PCA9552_create(const uint8_t deviceAddress, struct i2c_inst *i2c_use) {
  PCA9552 *pca = (PCA9552 *)malloc(sizeof(PCA9552));
  pca->address = deviceAddress;
  pca->i2c = i2c_use;
  pca->error = 0;
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      pca->leds[i][j] = 0;
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    PCA9552_writeReg(pca, PCA9552_LS0, 0x55);
  }
  PCA9552_setPrescaler(pca, 0, 0);  //  44 Hz
  PCA9552_setPrescaler(pca, 1, 0);  //  44 Hz
  PCA9552_setPWM(pca, 0, 247);      // dim
  PCA9552_setPWM(pca, 1, 247);
  return pca;
}