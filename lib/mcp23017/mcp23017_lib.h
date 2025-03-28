#ifndef mcp23017_lib_file
#define mcp23017_lib_file

// Include the pico i2c library
#include "hardware/i2c.h"

/*
 * Initializes the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The function returns 0 if the initialization was successful.
 * The function returns 1 if the initialization was unsuccessful.
 */
uint8_t mcp23017_init(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets the direction of GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in dir corresponds to their pins on GPIO A.
 * For each bit in dir:
 * - A value of 0 indicates the pin is an output.
 * - A value of 1 indicates the pin is an input.
 */
void mcp23017_set_dir_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t dir);

/*
 * Returns the direction of GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates the pin is an output.
 * - A value of 1 indicates the pin is an input.
 */
uint8_t mcp23017_get_dir_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets the direction of GPIO B.  See mcp23017_set_dir_gpioa for more details.
 */
void mcp23017_set_dir_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t dir);

/*
 * Returns the direction of GPIO A.  See mcp23017_get_dir_gpioa for more details.
 */
uint8_t mcp23017_get_dir_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Sets the direction of a pin on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting at 0.
 *
 * A value of 0 on dir indicates the pin is an output.
 * A value of 1 on dir indicates the pin is an input.
 */
void mcp23017_set_dir_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool dir);

/*
 * Returns the direction of a pin on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to get the state of is specified by pin_num starting at 0.
 *
 * A returned value of 0 indicates the pin is an output.
 * A returned value of 1 indicates the pin is an input.
 */
bool mcp23017_get_dir_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Sets the direction of a pin on GPIO B.  See mcp23017_set_dir_gpioa_pin for more details.
 */
void mcp23017_set_dir_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool dir);

/*
 * Returns the direction of a pin on GPIO A.  See mcp23017_get_dir_gpio_pin for more details.
 */
bool mcp23017_get_dir_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Sets the input polarity of GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in polarity corresponds to their pins on GPIO A.
 * For each bit in polarity:
 * - A value of 0 indicates future pin reads returns the value on the input pin.
 * - A value of 1 indicates future pin reads returns the opposite value on the input pin.
 */
void mcp23017_set_polarity_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t polarity);

/*
 * Returns the input polarity of GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates future pin reads returns the value on the input pin.
 * - A value of 1 indicates future pin reads returns the opposite value on the input pin.
 */
uint8_t mcp23017_get_polarity_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets the input polarity of GPIO B.  See mcp23017_set_polarity_gpioa for more details.
 */
void mcp23017_set_polarity_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t polarity);

/*
 * Returns the input polarity of GPIO B.  See mcp23017_get_dir_gpioa for more details.
 */
uint8_t mcp23017_get_polarity_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Sets the input polarity of a pin on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting at 0.
 *
 * A value of 0 indicates future pin reads returns the value on the input pin.
 * A value of 1 indicates future pin reads returns the opposite value on the input pin.
 */
void mcp23017_set_polarity_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool polarity);

/*
 * Returns the input polarity of a pin on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to get is specified by pin_num starting at 0.
 *
 * A returned value of 0 indicates future pin reads returns the value on the input pin.
 * A returned value of 1 indicates future pin reads returns the opposite value on the input pin.
 */
bool mcp23017_get_polarity_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Sets the input polarity of a pin on GPIO B.  See mcp23017_set_polarity_gpioa_pin for more details.
 */
void mcp23017_set_polarity_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool polarity);

/*
 * Returns the input polarity of a pin on GPIO B.  See mcp23017_get_polarity_gpioa_pin for more details.
 */
bool mcp23017_get_polarity_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Enables or disables the interrupt on change functionality of GPIO A for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in int_on_change corresponds to their pins on GPIO A.
 * For each in int_on_change:
 * - A value of 0 indicates the pin has interrupt on change disabled
 * - A value of 1 indicates the pin has interrupt on change enabled
 */
void mcp23017_set_int_on_change_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t int_on_change);

/*
 * Returns the state of the interrupt on change functionality of GPIO A for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates the pin has interrupt on change disabled
 * - A value of 1 indicates the pin has interrupt on change enabled
 */
uint8_t mcp23017_get_int_on_change_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Enables or disables the interrupt on change functionality of GPIO B.
 * See mcp23017_set_int_on_change_gpioa for more details.
 */
void mcp23017_set_int_on_change_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t int_on_change);

/*
 * Returns the state of the interrupt on change functionality of GPIO B.
 * See mcp23017_get_int_on_change_gpioa for more details.
 */
uint8_t mcp23017_get_int_on_change_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Enables or disables the interrupt on change functionality for a pin on GPIO A
 * for the MCP23017 at the address specified by mcp23017_addr using the i2c hardware
 * peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 *
 * A value of 0 on int_on_change indicates the pin has interrupt on change disabled
 * A value of 1 on int_on_change indicates the pin has interrupt on change enabled
 */
void mcp23017_set_int_on_change_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool int_on_change);

/*
 * Returns the state of the interrupt on change functionality of a pin on GPIO A
 * for the MCP23017 at the address specified by mcp23017_addr using the i2c hardware
 * peripheral specified by hardware_i2c.
 *
 * The pin to get is specified by pin_num starting at 0.
 *
 * A returned value of 0 indicates the pin has interrupt on change disabled
 * A returned value of 1 indicates the pin has interrupt on change enabled
 */
bool mcp23017_get_int_on_change_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Enables or disables the interrupt on change functionality of a pin GPIO B.
 * See mcp23017_set_int_on_change_gpioa_pin for more details.
 */
void mcp23017_set_int_on_change_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool int_on_change);

/*
 * Returns the state of the interrupt on change functionality of a pin GPIO B.
 * See mcp23017_get_int_on_change_gpioa_pin for more details.
 */
bool mcp23017_get_int_on_change_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Configures the interrupt comparison value for GPIO A for the MCP23017 at the address specified by
 * mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in default_compare corresponds to their pins on GPIO A.
 * If the pin is different compared to its corresponding bit, an interrupt is triggered.
 */
void mcp23017_set_default_int_compare_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t default_compare);

/*
 * Returns the interrupt comparison value for GPIO A for the MCP23017 at the address specified by
 * mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * If the pin is different compared to its corresponding bit, an interrupt is triggered.
 */
uint8_t mcp23017_get_default_int_compare_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Configures the interrupt comparison value for  GPIO B.
 * See mcp23017_set_default_int_compare_gpioa for more details.
 */
void mcp23017_set_default_int_compare_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t default_compare);

/*
 * Returns the interrupt comparison value for GPIO B.
 * See mcp23017_get_default_int_compare_gpioa for more details.
 */
uint8_t mcp23017_get_default_int_compare_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Configures the interrupt comparison value for a pin for GPIO A for the MCP23017 at the
 * address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 *
 * If the pin is different compared to its corresponding bit, an interrupt is triggered.
 */
void mcp23017_set_default_int_compare_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool default_compare);

/*
 * Returns the interrupt comparison value for a pin for GPIO A for the MCP23017 at the
 * address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting at 0.
 *
 * If the pin is different compared to its corresponding bit, an interrupt is triggered.
 */
bool mcp23017_get_default_int_compare_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Configures the interrupt comparison value for a pin for GPIO B.
 * See value for mcp23017_set_default_int_compare_gpioa_pin more details.
 */
void mcp23017_set_default_int_compare_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool default_compare);

/*
 * Returns the interrupt comparison value for a pin for GPIO B.
 * See mcp23017_get_default_int_compare_gpioa_pin for more details.
 */
bool mcp23017_get_default_int_compare_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Set which value the pins on GPIO A is compared against to trigger an interrupt for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in int_control corresponds to their pins on GPIO A.
 * For each in int_control:
 * - A value of 0 indicates the pin is compared against the previous pin value.
 * - A value of 1 indicates the pin is compared against the associated bit in the default control register.
 */
void mcp23017_set_int_control_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t int_control);

/*
 * Get which value the pins on GPIO A is compared against to trigger an interrupt for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates the pin is compared against the previous pin value.
 * - A value of 1 indicates the pin is compared against the associated bit in the default control register.
 */
uint8_t mcp23017_get_int_control_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Set which value is the pins on GPIO B is compared against.  See mcp23017_set_int_control_gpioa for more details.
 */
void mcp23017_set_int_control_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t int_control);

/*
 * Get which value is the pins on GPIO B is compared against.  See mcp23017_get_int_control_gpioa for more details.
 */
uint8_t mcp23017_get_int_control_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Set which value the given pin on GPIO A is compared against to trigger an interrupt for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 *
 * A value of 0 indicates the pin is compared against the previous pin value.
 * A value of 1 indicates the pin is compared against the associated bit in the default control register.
 */
void mcp23017_set_int_control_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool int_control);

/*
 * Get which value the given pin on GPIO A is compared against to trigger on interrupt for the MCP23017
 * at the address specified by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c
 *
 *The pin to set is specified by pin_num starting at 0.
 *
 * A returned value of 0 indicates the pin is compared against the previous pin value.
 * A returned value of 1 indicates the pin is compared against the associated bit in the default control register.
  */
bool mcp23017_get_int_control_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Sets which value the given pin on GPIO B is compared against to trigger an interrupt.
 * See mcp23017_set_int_control_gpioa_pin for more details.
 */
void mcp23017_set_int_control_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool int_control);

/*
 * Gets which value the given pin on GPIO B is compared against to trigger an interrupt.
 * See mcp23017_get_int_control_gpioa_pin for more details.
 */
bool mcp23017_get_int_control_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Enables or disables the pullup resistors on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in pullup_state corresponds to their pins on GPIO A.
 * For each in pullup_state:
 * - A value of 0 disables the pin's pullup resistor
 * - A value of 1 enables the pin's pullup resistor
 */
void mcp23017_set_pullup_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pullup_state);

/*
 * Get the state of the pullup resistors on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates the pin has it's pullup resistor disabled
 * - A value of 1 indicates the pin has it's pullup resistor enabled
 */
uint8_t mcp23017_get_pullup_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Enables or disables the pullup resistor on GPIO B.  See mcp23017_set_pullup_gpioa for more details.
 */
void mcp23017_set_pullup_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pullup_state);

/*
 * Get the states of the pullup resistors on GPIO B.  See mcp23017_get_pullup_gpioa for more details.
 */
uint8_t mcp23017_get_pullup_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Enables or disables a pullup resistor for a pin on GPIO A for the MCP23017 at the address specified
 * by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 *
 * A value of 0 on pullup_state disables the pin's pullup resistor
 * A value of 1 on pullup_state enables the pin's pullup resistor
 */
void mcp23017_set_pullup_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool pullup_state);

/*
 * Returns the state of the pullup resistor for a pin on GPIO A for the MCP23017 at the address specified
 * by mcp23017_addr using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to get is specified by pin_num starting at 0.
 *
 * A returned value of 0 indicates the pin has it's pullup resistor disabled
 * A returned value of 1 indicates the pin has it's pull up resistor enabled
  */
bool mcp23017_get_pullup_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Enables or disables a pullup resistor for a pin on GPIO B.  See mcp23017_set_pullup_gpioa_pin for more details.
 */
void mcp23017_set_pullup_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool pullup_state);

/*
 * Returns the state of the pullup resistor for a pin on GPIO B.  See mcp23017_get_pullup_gpioa_pin for more details.
 */
bool mcp23017_get_pullup_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Returns the interrupt state for each pin on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 * For each bit:
 * - A value of 0 indicates the pin has not had an interrupt happen
 * - A value of 1 indicates the pin has had an interrupt happen
 */
uint8_t mcp23017_get_int_flag_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Returns the value of the pins on GPIO A when an interrupt occurred for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 */
uint8_t mcp23017_get_int_captured_val_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Returns the interrupt state for each pin on GPIO B.  See mcp23017_get_int_flag_gpioa for more details.
 */
uint8_t mcp23017_get_int_flag_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Returns the value of the pins on GPIO B when an interrupt occurred.  See mcp23017_get_int_captured_val_gpioa for more details.
 */
uint8_t mcp23017_get_int_captured_val_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Returns the interrupt state for the specified pin for GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 *
 * A value of 0 on pin_num indicates the pin has not had an interrupt happen
 * A value of 1 on pin_num indicates the pin has had an interrupt happen
 */
bool mcp23017_get_int_flag_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Returns the value on the specified pin when an interrupt occurred on GPIO A. for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c
 *
 * The pin to set is specified by pin_num starting at 0.
  */
bool mcp23017_get_int_captured_val_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Returns the interrupt state for the specified pin for GPIO B.  See mcp23017_get_int_flag_gpioa_pin for more details.
 */
bool mcp23017_get_int_flag_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Returns the value on the specified pin when an interrupt occurred on GPIO A.  See mcp23017_get_int_captured_val_gpioa_pin for more details.
 */
bool mcp23017_get_int_captured_val_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Sets the output pins to the specified value on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in pin_state corresponds to their pins on GPIO A.
 */
void mcp23017_set_pins_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_state);

/*
 * Gets the current input state of the pins on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * Each bit in the returned value corresponds to their pins on GPIO A.
 */
uint8_t mcp23017_get_pins_gpioa(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets the output pins to the specified value on GPIO B.  See mcp23017_set_pins_gpioa for more details.
 */
void mcp23017_set_pins_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_state);

/*
 * Gets the current input state of the pins on GPIO B.  See mcp23017_get_pins_gpioa for more details.
 */
uint8_t mcp23017_get_pins_gpiob(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

// Only compile in the individual pin functions if needed.
#ifdef MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS
/*
 * Sets the specified pin to the given pin_state on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting 0.
 */
void mcp23017_set_pins_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool pin_state);

/*
 * Gets the specified pin state on GPIO A for the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * The pin to set is specified by pin_num starting at 0.
 */
bool mcp23017_get_pins_gpioa_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);

/*
 * Sets the specified pin to the given pin_state on GPIO B.  See mcp23017_set_pins_gpioa_pin for more details.
 */
void mcp23017_set_pins_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num, bool pin_state);

/*
 * Gets the specified state on GPIO B.  See mcp23017_get_pins_gpioa_pin for more details.
 */
bool mcp23017_get_pins_gpiob_pin(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, uint8_t pin_num);
#endif

/*
 * Enables or disables the interrupt mirroring on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If is_int_mirror is true, the two interrupt lines are mirrored.
 * If is_int_mirror is false, the two interrupt lines are not mirrored.
 */
void mcp23017_set_int_mirror(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool is_int_mirror);

/*
 * Returns the state of the two interrupt lines on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If the returned value is true, the two interrupt lines are mirrored.
 * If the returned value is false, the two interrupt lines are nor mirrored.
 */
bool mcp23017_get_int_mirror(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets if the interrupt lines are open drain on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If is_open_drain is true, the interrupt lines are configured as open drain.
 * If is_open_drain is false, the interrupt lines are not configured as open drain.
 */
void mcp23017_set_int_open_drain(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool is_open_drain);

/*
 * Returns the output configuration of the interrupt lines on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If the returned value is true, the interrupt lines are configured as open drain.
 * If the returned value is false, the interrupt lines are not configured as open drain.
 */
bool mcp23017_get_int_open_drain(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

/*
 * Sets the interrupt polarity on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If int_polarity is 1, the interrupt lines are configured as active-high.
 * If int_polarity is 0, the interrupt lines are configured as active-low.
 */
void mcp23017_set_int_polarity(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr, bool int_polarity);

/*
 * Returns the interrupt polarity on the MCP23017 at the address specified by mcp23017_addr
 * using the i2c hardware peripheral specified by hardware_i2c.
 *
 * If the returned value is 1, the interrupt lines are configured as active-high
 * If the returned value is 0, the interrupt lines are configured as active-low
 */
bool mcp23017_get_int_polarity(i2c_inst_t *hardware_i2c, uint8_t mcp23017_addr);

#endif
