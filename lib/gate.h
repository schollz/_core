// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef GATE_LIB
#define GATE_LIB 1
#define GATE_MAX 245.0

#include "utils.h"

typedef struct Gate {
  uint32_t threshold : 31;
  uint32_t active : 1;
  uint32_t counter;
  uint32_t blocks_per_second;
  float bpm;
  float setpoint;
} Gate;

void Gate_reset(Gate *gate) { gate->counter = 0; }

void Gate_update_threshold(Gate *gate) {
  if (gate->setpoint < GATE_MAX) {
    gate->threshold = (uint32_t)round(gate->blocks_per_second * gate->setpoint *
                                      30.0 / (gate->bpm * GATE_MAX));
  }
}

Gate *Gate_create(uint32_t blocks_per_second, float bpm) {
  Gate *gate = (Gate *)malloc(sizeof(Gate));
  gate->blocks_per_second = blocks_per_second;
  gate->setpoint = GATE_MAX;
  gate->bpm = bpm;
  gate->active = true;
  Gate_reset(gate);
  return gate;
}

void Gate_update(Gate *gate, float bpm) {
  if (!gate->active) {
    return;
  }
  if (gate->counter < gate->threshold) {
    if (gate->bpm != bpm) {
      gate->bpm = bpm;
      Gate_update_threshold(gate);
    }
    gate->counter++;
  }
}

void Gate_set_amount(Gate *gate, float setpoint) {
  gate->setpoint = util_clamp(setpoint, 0, GATE_MAX);
  Gate_update_threshold(gate);
}

bool Gate_is_up(Gate *gate) {
  if (!gate->active) {
    return false;
  }
  if (gate->setpoint >= GATE_MAX) {
    return false;
  }
  return gate->counter >= gate->threshold;
}

void Gate_set_active(Gate *gate, bool active) { gate->active = active; }
void Gate_destroy(Gate *gate) { free(gate); }

#endif /* GATE_LIB */
#define GATE_LIB 1