// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef BANK_TRANSITION_H
#define BANK_TRANSITION_H
#include <stdatomic.h>
enum BankTransitionState {
    BANK_IDLE, BANK_REQUESTED, BANK_FADING_OUT, BANK_LOADING,
    BANK_COMMITTING, BANK_ROLLBACK, BANK_ERROR, BANK_CANCEL
};
static _Atomic unsigned bank_transition_state;
static _Atomic bool bank_fade_done, bank_fade_silent;
static bool bank_transition_busy(void) {
    return atomic_load_explicit(&bank_transition_state,memory_order_acquire)!=BANK_IDLE;
}
static bool bank_transition_audio_hold(void) {
    return atomic_load_explicit(&bank_fade_done,memory_order_acquire);
}
static bool bank_transition_fading(void) {
    return atomic_load_explicit(&bank_transition_state,memory_order_acquire)==BANK_FADING_OUT;
}
static void bank_transition_service(void);
#endif
