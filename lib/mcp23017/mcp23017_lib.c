// Include the needed lines
#include "mcp23017_lib.h"
#include "hardware/i2c.h"

// Declare the macros which will be used to generate all the functions.

// This function is used to write an entire value to the specified register address
#define MCP23017_LIB_WRITE_VALUE_TO_REG(FUNCTION_NAME, REG_ADDR, DATA_NAME) \
void FUNCTION_NAME(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t DATA_NAME)\
{\
    mcp23017_write_reg(hardware_i2c, mcp23017_addr, REG_ADDR, DATA_NAME);\
}

// Only include the pin level functions if configured to do so.
// To write a specific bit, read the current state then change only the specified bit.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
#define MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(FUNCTION_NAME, REG_ADDR, DATA_NAME) \
void FUNCTION_NAME(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool DATA_NAME) \
{\
    uint8_t current_reg_state = mcp23017_read_reg(hardware_i2c, mcp23017_addr, REG_ADDR);\
    mcp23017_write_reg(hardware_i2c, mcp23017_addr, REG_ADDR, (uint8_t)(current_reg_state & ~(1 << pin_num)) | (uint8_t)(DATA_NAME << pin_num)); \
}
#else
#define MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(FUNCTION_NAME, REG_ADDR, DATA_NAME)
#endif

// This function is used to read an entire value from the specified address
#define MCP23017_LIB_READ_VALUE_FROM_REG(FUNCTION_NAME, REG_ADDR) \
uint8_t FUNCTION_NAME(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr)\
{\
    return mcp23017_read_reg(hardware_i2c, mcp23017_addr, REG_ADDR);\
}

// Only include the pin level functions if configured to do so.
// To read a single bit, read the entire register then pick out the desired bit.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
#define MCP23017_LIB_READ_VALUE_FROM_REG_PIN(FUNCTION_NAME, REG_ADDR) \
bool FUNCTION_NAME(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num) \
{\
    return mcp23017_read_reg(hardware_i2c, mcp23017_addr, REG_ADDR) & (1 << pin_num); \
}
#else
#define MCP23017_LIB_READ_VALUE_FROM_REG_PIN(FUNCTION_NAME, REG_ADDR)
#endif

// Function to read from the given register.
uint8_t mcp23017_read_reg(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t reg_addr)
{
    i2c_write_blocking(hardware_i2c, mcp23017_addr, &reg_addr, 1, true);

    uint8_t data_read;
    i2c_read_blocking(hardware_i2c, mcp23017_addr, &data_read, 1, false);
    return data_read;
}

// Function to write to the given register.
inline void mcp23017_write_reg(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t reg_addr, uint8_t data_write_to_reg)
{
    uint8_t data_write[2] = {reg_addr, data_write_to_reg};
    i2c_write_blocking(hardware_i2c, mcp23017_addr, data_write, 2, false);
}

// Create all the functions

uint8_t mcp23017_init(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr)
{
    if (hardware_i2c == NULL || mcp23017_addr < 0b0100000 || mcp23017_addr > 0b0100111) {
        return 1;
    }
    return 0;
}

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_dir_gpioa, 0x00, dir)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_dir_gpioa_pin, 0x00, dir)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_dir_gpioa, 0x00)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_dir_gpioa_pin, 0x00)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_dir_gpiob, 0x01, dir)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_dir_gpiob_pin, 0x01, dir)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_dir_gpiob, 0x01)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_dir_gpiob_pin, 0x01)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_polarity_gpioa, 0x02, polarity)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_polarity_gpioa_pin, 0x02, polarity)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_polarity_gpioa, 0x02)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_polarity_gpioa_pin, 0x02)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_polarity_gpiob, 0x03, polarity)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_polarity_gpiob_pin, 0x03, polarity)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_polarity_gpiob, 0x03)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_polarity_gpiob_pin, 0x03)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_int_on_change_gpioa, 0x04, int_on_change)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_int_on_change_gpioa_pin, 0x04, int_on_change)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_on_change_gpioa, 0x04)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_on_change_gpioa_pin, 0x04)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_int_on_change_gpiob, 0x05, int_on_change)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_int_on_change_gpiob_pin, 0x05, int_on_change)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_on_change_gpiob, 0x05)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_on_change_gpiob_pin, 0x05)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_default_int_compare_gpioa, 0x06, default_compare)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_default_int_compare_gpioa_pin, 0x06, default_compare)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_default_int_compare_gpioa, 0x06)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_default_int_compare_gpioa_pin, 0x06)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_default_int_compare_gpiob, 0x07, default_compare)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_default_int_compare_gpiob_pin, 0x07, default_compare)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_default_int_compare_gpiob, 0x07)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_default_int_compare_gpiob_pin, 0x07)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_int_control_gpioa, 0x08, int_control)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_int_control_gpioa_pin, 0x08, int_control)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_control_gpioa, 0x08)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_control_gpioa_pin, 0x08)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_int_control_gpiob, 0x09, int_control)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_int_control_gpiob_pin, 0x09, int_control)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_control_gpiob, 0x09)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_control_gpiob_pin, 0x09)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_pullup_gpioa, 0x0c, pullup_state)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_pullup_gpioa_pin, 0x0c, pullup_state)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_pullup_gpioa, 0x0c)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_pullup_gpioa_pin, 0x0c)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_pullup_gpiob, 0x0d, pullup_state)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_pullup_gpiob_pin, 0x0d, pullup_state)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_pullup_gpiob, 0x0d)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_pullup_gpiob_pin, 0x0d)

MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_flag_gpioa, 0x0e)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_flag_gpioa_pin, 0x0e)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_int_captured_val_gpioa, 0x10)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_int_captured_val_gpioa_pin, 0x10)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_mcp23017_get_int_flag_gpiob, 0x0f)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_mcp23017_get_int_flag_gpiob_pin, 0x0f)
MCP23017_LIB_READ_VALUE_FROM_REG(get_int_captured_val_gpiob, 0x11)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(get_int_captured_val_gpiob_pin, 0x11)

MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_pins_gpioa, 0x12, pin_state)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_pins_gpioa_pin, 0x12, pin_state)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_pins_gpioa, 0x12)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_pins_gpioa_pin, 0x12)
MCP23017_LIB_WRITE_VALUE_TO_REG(mcp23017_set_pins_gpiob, 0x13, pin_state)
MCP23017_LIB_WRITE_VALUE_TO_REG_PIN(mcp23017_set_pins_gpiob_pin, 0x13, pin_state)
MCP23017_LIB_READ_VALUE_FROM_REG(mcp23017_get_pins_gpiob, 0x13)
MCP23017_LIB_READ_VALUE_FROM_REG_PIN(mcp23017_get_pins_gpiob_pin, 0x13)

void mcp23017_set_int_mirror(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool is_int_mirror)
{
    uint8_t current_reg_state = mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a);
    mcp23017_write_reg(hardware_i2c, mcp23017_addr, 0x0a, (current_reg_state & 0b10111111) | (uint8_t)(is_int_mirror << 6));
}

bool mcp23017_get_int_mirror(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr)
{
    return mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a) & 0b01000000;
}

void mcp23017_set_int_open_drain(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool is_open_drain)
{
    uint8_t current_reg_state = mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a);
    mcp23017_write_reg(hardware_i2c, mcp23017_addr, 0x0a, (current_reg_state & 0b11111011) | (uint8_t)(is_open_drain << 2));
}

bool mcp23017_get_int_open_drain(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr)
{
    return mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a) & 0b00000100;
}

void mcp23017_set_int_polarity(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool int_polarity)
{
    uint8_t current_reg_state = mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a);
    mcp23017_write_reg(hardware_i2c, mcp23017_addr, 0x0a, (uint8_t)(current_reg_state & 0b11111101) | (uint8_t)(int_polarity << 1));
}

bool mcp23017_get_int_polarity(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr)
{
    return mcp23017_read_reg(hardware_i2c, mcp23017_addr, 0x0a) & 0b00000010;
}
