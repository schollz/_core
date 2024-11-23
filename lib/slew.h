#ifndef SLEW_LIB
#define SLEW_LIB 1

#include <stdint.h>
#include <stdlib.h>

typedef struct Slew {
  float current;                 // Current value of the parameter
  float target;                  // Target value to slew towards
  float step;                    // Step size per sample
  unsigned int remaining_steps;  // Remaining steps to reach the target
} Slew;

/**
 * Initialize a Slew instance.
 * @param slew Pointer to the Slew instance.
 * @param steps The number of samples over which to slew.
 * @param initial_value The initial value of the parameter.
 */
void Slew_init(Slew *slew, unsigned int steps, float initial_value) {
  slew->current = initial_value;
  slew->target = initial_value;
  slew->step = 0.0f;
  slew->remaining_steps = 0;
}

/**
 * Set a new target value for the Slew instance.
 * @param slew Pointer to the Slew instance.
 * @param target The new target value.
 * @param steps The number of samples over which to transition to the target.
 */
void Slew_set_target(Slew *slew, float target, unsigned int steps) {
  slew->target = target;
  slew->remaining_steps = steps;
  if (steps > 0) {
    slew->step = (target - slew->current) / (float)steps;
  } else {
    slew->current = target;  // Instant transition if steps == 0
    slew->step = 0.0f;
  }
}

/**
 * Process a Slew instance for one sample.
 * @param slew Pointer to the Slew instance.
 * @return The current value after slewing.
 */
float Slew_process(Slew *slew) {
  if (slew->remaining_steps > 0) {
    slew->current += slew->step;
    slew->remaining_steps--;
  } else {
    slew->current = slew->target;
  }
  return slew->current;
}

#endif
