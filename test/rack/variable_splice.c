// Exercise the generated production scheduler, without a second implementation.
// Private access is limited to establishing jumps/reverse/suppression states and
// observing phase publication, which the desktop display cannot expose.
#ifndef VARIABLE_ENGINE_SOURCE
#define VARIABLE_ENGINE_SOURCE <engine.c>
#endif
#include VARIABLE_ENGINE_SOURCE
#include <assert.h>
#include <stdbool.h>

typedef struct {
    CoreEngine *engine;
    int32_t starts[8], stops[8];
    int8_t types[8];
    unsigned count;
} Fixture;

static bool pcm(void *user, unsigned bank, unsigned slot, unsigned variant,
                uint64_t offset, void *out, size_t bytes) {
    (void)user; (void)bank; (void)slot; (void)offset;
    assert(variant == 0);
    int16_t *samples = out;
    for (size_t i = 0; i < bytes / 2; ++i) samples[i] = 1000;
    return true;
}

static CoreDisplay display(Fixture *f) {
    CoreDisplay d;
    core_engine_display(f->engine, &d);
    return d;
}

static void initialize(Fixture *f, const unsigned *ticks, unsigned count,
                       unsigned splice, unsigned channels, unsigned rate) {
    memset(f, 0, sizeof *f);
    f->count = count;
    for (unsigned i = 0; i < count; ++i) {
        f->starts[i] = i ? f->stops[i - 1] : 0;
        unsigned frames = (unsigned)lround(ticks[i] * (44100.0 * rate / 384.0));
        f->stops[i] = f->starts[i] + frames * channels * 2;
    }
    f->engine = core_engine_create(123);
    assert(f->engine);
    CoreBank banks[16] = {0};
    CoreSample sample = {.size = f->stops[count - 1], .bpm = 120,
        .splice_ticks = splice, .channels = channels, .rate_multiple = rate,
        .tempo_match = true, .variable = true, .slice_count = count,
        .starts = f->starts, .stops = f->stops, .types = f->types};
    banks[0].count = 2;
    banks[0].samples[0] = sample;
    sample.slot = 1;
    banks[0].samples[1] = sample;
    banks[1].count = 1;
    sample.slot = 0;
    banks[1].samples[0] = sample;
    core_engine_set_catalogue(f->engine, banks, pcm, 0);
    core_engine_set_resident_bank(f->engine, 0);
    CoreControls controls = {.knobs = {0, 0, 0, .5f, 0}};
    core_engine_controls(f->engine, &controls);
    CoreState state;
    core_engine_get_state(f->engine, &state);
    state.tempo = 120;
    state.stopped = true;
    state.muted = false;
    state.clock_stop = true;
    state.jump_probability = state.retrigger_probability = state.sequence_length = 0;
    memset(state.effects, 0, sizeof state.effects);
    memset(state.runes, 0, sizeof state.runes);
    assert(core_engine_set_state(f->engine, &state));
    for (unsigned i = 0; i < CORE_RATE; ++i) {
        int16_t out[2];
        core_engine_process(f->engine, out);
    }
}

static void start(Fixture *f, bool clocked) {
    if (clocked) {
        core_engine_clock(f->engine, true);
    } else {
        CoreState state;
        core_engine_get_state(f->engine, &state);
        state.stopped = false;
        assert(core_engine_set_state(f->engine, &state));
        while (f->engine->s_do_restart_playback) {
            int16_t out[2];
            core_engine_process(f->engine, out);
        }
    }
    assert(display(f).slice == 0);
}

static CoreDisplay frame(Fixture *f, unsigned elapsed, bool clocked, int16_t out[2]) {
    if (clocked) core_engine_clock(f->engine, elapsed % (CORE_RATE / 4) < 200);
    core_engine_process(f->engine, out);
    return display(f);
}

static void check_durations(const unsigned *ticks, unsigned count, unsigned splice,
                            unsigned channels, unsigned rate, bool clocked) {
    Fixture f;
    initialize(&f, ticks, count, splice, channels, rate);
    start(&f, clocked);
    unsigned last = 0, events = 0, accumulated = ticks[0];
    const double frames_per_tick = CORE_RATE / 384.0;
    for (unsigned elapsed = 0; elapsed < CORE_RATE * 4; ++elapsed) {
        int16_t out[2];
        CoreDisplay d = frame(&f, elapsed, clocked, out);
        if (d.slice == last) continue;
        ++events;
        double expected = accumulated * frames_per_tick;
        if (d.slice != events % count || fabs((elapsed + 1) - expected) > frames_per_tick + 2) {
            fprintf(stderr, "variable: splice=%u channels=%u rate=%u clock=%u event=%u "
                "slice=%u frames=%u expected=%.2f\n", splice, channels, rate, clocked,
                events, d.slice, elapsed + 1, expected);
            assert(false);
        }
        accumulated += ticks[d.slice];
        last = d.slice;
    }
    assert(events >= count * 2);
    core_engine_destroy(f.engine);
}

// Advance the real timer with audio acknowledging the previous phase. This
// observes retriggers of the same slice, even when the display stays constant.
static bool timer_tick(Fixture *f, bool pulse) {
    CoreEngine *old = core_enter(f->engine);
    f->engine->s_host_time += f->engine->s_timer.interval;
    f->engine->s_phase_change = false;
    if (pulse) clock_handling_up(250000);
    timer_step();
    bool published = f->engine->s_phase_change;
    core_leave(old);
    return published;
}

static void manual_fixture(Fixture *f, const unsigned *ticks, unsigned count) {
    initialize(f, ticks, count, 24, 2, 1);
    start(f, true);
    CoreState state;
    core_engine_get_state(f->engine, &state);
    state.clock_stop = false;
    assert(core_engine_update_settings(f->engine, &state));
}

static void jump(Fixture *f, unsigned slice) {
    CoreEngine *old = core_enter(f->engine);
    key_do_jump_to_slice(slice, 0);
    core_leave(old);
}

static void check_single_and_short(void) {
    const unsigned ticks[] = {48};
    Fixture f;
    manual_fixture(&f, ticks, 1);
    for (unsigned i = 1; i <= 480; ++i)
        assert(timer_tick(&f, i % 96 == 0) == (i % 48 == 0));
    core_engine_destroy(f.engine);
    manual_fixture(&f, ticks, 1);
    f.stops[0] = 4; // One stereo frame rounds below a tick, but must make progress.
    for (unsigned i = 0; i < 10; ++i) assert(timer_tick(&f, false));
    core_engine_destroy(f.engine);
}

static void check_jump_reverse_and_suppression(void) {
    enum { SCRATCH = 11 }; // lib/definitions.h; generated C has expanded macros.
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    manual_fixture(&f, ticks, 4);
    for (unsigned i = 0; i < 24; ++i) assert(!timer_tick(&f, false));
    jump(&f, 2);
    // Manual jumps retain the existing one-transition debounce. The whole
    // slice still plays before that extra tick, rather than inheriting old time.
    for (unsigned i = 1; i <= 97; ++i) assert(timer_tick(&f, false) == (i == 97));
    assert(display(&f).slice == 3); // The jump receives all 96 ticks, not the remainder.
    f.engine->s_phase_forward = false;
    jump(&f, 0);
    for (unsigned i = 1; i <= 49; ++i) assert(timer_tick(&f, false) == (i == 49));
    assert(display(&f).slice == 3);
    for (unsigned i = 1; i <= 192; ++i) assert(timer_tick(&f, false) == (i == 192));
    assert(display(&f).slice == 2);
    f.engine->s_phase_forward = true;
    jump(&f, 0);
    f.engine->s_sf->fx_active[SCRATCH] = true;
    for (unsigned i = 0; i < 60; ++i) assert(!timer_tick(&f, false));
    assert(display(&f).slice == 0);
    f.engine->s_sf->fx_active[SCRATCH] = false;
    assert(timer_tick(&f, false));
    assert(display(&f).slice == 1); // A blocked transition cannot skip logical beats.
    for (unsigned i = 1; i <= 144; ++i) assert(timer_tick(&f, false) == (i == 144));
    f.engine->s_sf->fx_active[SCRATCH] = true;
    f.engine->s_do_restart_playback = true;
    assert(!timer_tick(&f, false));
    assert(!timer_tick(&f, false));
    f.engine->s_sf->fx_active[SCRATCH] = false;
    assert(timer_tick(&f, false));
    assert(display(&f).slice == 0); // A blocked restart survives until publication.
    core_engine_destroy(f.engine);
}

static void check_selection_and_tempo(void) {
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    manual_fixture(&f, ticks, 4);
    for (unsigned i = 0; i < 35; ++i) assert(!timer_tick(&f, false));
    f.engine->s_sel_sample_cur = f.engine->s_sel_sample_next = 1;
    for (unsigned i = 1; i <= 48; ++i) assert(timer_tick(&f, false) == (i == 48));
    assert(display(&f).slice == 1);
    for (unsigned i = 0; i < 24; ++i) assert(!timer_tick(&f, false));
    f.engine->s_sf->bpm_tempo = 60;
    uint64_t before = f.engine->s_host_time;
    for (unsigned i = 1; i <= 120; ++i) assert(timer_tick(&f, false) == (i == 120));
    assert(display(&f).slice == 2 && display(&f).tempo == 60);
    assert(llabs((int64_t)(f.engine->s_host_time - before) - 625000) < 5300);
    core_engine_set_resident_bank(f.engine, 1);
    for (unsigned i = 1; i <= 48; ++i) assert(timer_tick(&f, false) == (i == 48));
    assert(display(&f).bank == 1 && display(&f).slice == 3);
    core_engine_destroy(f.engine);
    manual_fixture(&f, ticks, 4);
    for (unsigned i = 0; i < 35; ++i) assert(!timer_tick(&f, false));
    core_engine_set_resident_bank(f.engine, 0); // New generation, same selection.
    for (unsigned i = 1; i <= 48; ++i) assert(timer_tick(&f, false) == (i == 48));
    assert(display(&f).slice == 1);
    core_engine_destroy(f.engine);
}

static void check_selection_policies_and_priorities(void) {
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    manual_fixture(&f, ticks, 4);
    // The sequence's logical steps differ from the actual selected slices.
    const unsigned selected[] = {2, 0, 3, 1};
    f.engine->s_random_sequence_length = 4;
    for (unsigned i = 0; i < 4; ++i) f.engine->s_random_sequence_arr[i] = selected[i];
    CoreEngine *old = core_enter(f.engine);
    do_update_phase_from_beat_current();
    core_leave(old);
    assert(display(&f).slice == 2);
    for (unsigned step = 0; step < 8; ++step) {
        unsigned duration = ticks[selected[step % 4]];
        for (unsigned i = 1; i <= duration; ++i)
            assert(timer_tick(&f, i % 96 == 0) == (i == duration));
        assert(display(&f).slice == selected[(step + 1) % 4]);
    }
    core_engine_destroy(f.engine);
    for (unsigned snare = 0; snare < 2; ++snare) {
        manual_fixture(&f, ticks, 4);
        f.types[1] = 2; f.types[3] = 1;
        f.engine->s_only_play_kicks = !snare;
        f.engine->s_only_play_snares = snare;
        for (unsigned i = 1; i <= 48; ++i) assert(timer_tick(&f, false) == (i == 48));
        unsigned slice = snare ? 1 : 3;
        assert(display(&f).slice == slice);
        for (unsigned i = 1; i <= ticks[slice]; ++i)
            assert(timer_tick(&f, false) == (i == ticks[slice]));
        assert(display(&f).slice == slice);
        core_engine_destroy(f.engine);
    }
    manual_fixture(&f, ticks, 4);
    f.engine->s_retrig_beat_num = 1;
    f.engine->s_retrig_ready = f.engine->s_retrig_first = false;
    for (unsigned i = 0; i < 60; ++i) assert(!timer_tick(&f, false));
    f.engine->s_retrig_beat_num = 0;
    assert(timer_tick(&f, false) && display(&f).slice == 1);
    f.engine->s_sequencerhandler[0].playing = true;
    for (unsigned i = 0; i < 150; ++i) assert(!timer_tick(&f, false));
    f.engine->s_sequencerhandler[0].playing = false;
    assert(timer_tick(&f, false) && display(&f).slice == 2);
    f.engine->s_realtime_stretch_active = true;
    for (unsigned i = 0; i < 100; ++i) assert(!timer_tick(&f, false));
    f.engine->s_realtime_stretch_active = false;
    assert(timer_tick(&f, false) && display(&f).slice == 3);
    core_engine_destroy(f.engine);
    initialize(&f, ticks, 4, 24, 2, 1);
    f.engine->s_host_info[0][0].one_shot = true;
    start(&f, false);
    for (unsigned i = 0; i < 200; ++i) assert(!timer_tick(&f, false));
    start(&f, true); // Clocked one-shot samples retain their clock-driven transport.
    for (unsigned i = 1; i <= 48; ++i) assert(timer_tick(&f, false) == (i == 48));
    assert(display(&f).slice == 1);
    core_engine_destroy(f.engine);
}

static void check_loss_and_restart(bool stop) {
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    initialize(&f, ticks, 4, 24, 2, 1);
    CoreState state;
    core_engine_get_state(f.engine, &state);
    state.clock_stop = stop;
    assert(core_engine_update_settings(f.engine, &state));
    start(&f, true);
    for (unsigned elapsed = 0; elapsed < CORE_RATE * 3; ++elapsed) {
        int16_t out[2];
        frame(&f, elapsed, true, out);
    }
    core_engine_clock(f.engine, false);
    unsigned changes = 0, audible = 0, last = display(&f).slice;
    for (unsigned elapsed = 0; elapsed < CORE_RATE * 2; ++elapsed) {
        int16_t out[2];
        CoreDisplay d = frame(&f, elapsed, false, out);
        if (elapsed < CORE_RATE) { last = d.slice; continue; }
        changes += d.slice != last;
        audible += out[0] != 0 || out[1] != 0;
        if (stop) assert(!d.clock);
        last = d.slice;
    }
    assert(stop ? !changes && !audible : changes && audible);
    core_engine_clock(f.engine, true);
    assert(display(&f).slice == 0); // The edge publishes zero before rendering.
    for (unsigned elapsed = 0; elapsed < CORE_RATE / 4; ++elapsed) {
        int16_t out[2];
        CoreDisplay d = frame(&f, elapsed, true, out);
        if (elapsed < CORE_RATE / 8 - 120) assert(d.slice == 0);
        if (elapsed > CORE_RATE / 8 + 120) assert(d.slice == 1);
    }
    core_engine_destroy(f.engine);
}

static void check_tap_mode(bool next_edge) {
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    initialize(&f, ticks, 4, 24, 2, 1);
    start(&f, true);
    CoreControls controls = {.knobs = {0, 0, 0, .5f, 0}};
    for (unsigned elapsed = 0; elapsed < CORE_RATE * 11 / 10; ++elapsed) {
        if (elapsed == CORE_RATE * 76 / 100) {
            controls.buttons[3] = true;
            core_engine_controls(f.engine, &controls);
        }
        if (elapsed == CORE_RATE * (next_edge ? 91 : 83) / 100) {
            controls.buttons[0] = true;
            core_engine_controls(f.engine, &controls);
        }
        if (elapsed == CORE_RATE * 97 / 100) {
            controls.buttons[0] = controls.buttons[3] = false;
            core_engine_controls(f.engine, &controls);
        }
        int16_t out[2];
        CoreDisplay d = frame(&f, elapsed, true, out);
        if (!next_edge && elapsed == CORE_RATE * 88 / 100) assert(d.slice == 0);
        if (next_edge && elapsed == CORE_RATE * 98 / 100) assert(d.slice == 3);
        if (next_edge && elapsed == CORE_RATE * 101 / 100) assert(d.slice == 0);
    }
    core_engine_destroy(f.engine);
}

static void check_reset_during_stutter(void) {
    const unsigned ticks[] = {48, 144, 96, 192};
    Fixture f;
    manual_fixture(&f, ticks, 4);
    jump(&f, 2);
    CoreControls controls = {.knobs = {0, 0, 0, .5f, 0}};
    // Request the immediate TAP+MODE reset through the production controls,
    // while a stutter owns phase publication. Controls run every millisecond.
    for (unsigned elapsed = 0; elapsed < 100; ++elapsed) {
        controls.buttons[3] = elapsed < 80;
        controls.buttons[0] = elapsed >= 40 && elapsed < 80;
        core_engine_controls(f.engine, &controls);
        CoreEngine *old = core_enter(f.engine);
        f.engine->s_host_time += 1000;
        input_handling();
        core_leave(old);
    }
    f.engine->s_retrig_beat_num = 3;
    f.engine->s_retrig_ready = true;
    f.engine->s_retrig_first = false;
    f.engine->s_retrig_timer_reset = 1;
    assert(timer_tick(&f, false) && display(&f).slice == 2);
    f.engine->s_retrig_beat_num = 0;
    f.engine->s_key_jump_debounce = 0;
    assert(timer_tick(&f, false) && display(&f).slice == 0);
    core_engine_destroy(f.engine);
}

int main(void) {
    const unsigned uneven[] = {48, 144, 96, 192}, fast[] = {12, 24, 36, 312};
    for (int clocked = 1; clocked >= 0; --clocked)
        for (unsigned splice = 24; splice <= 96; splice *= 4)
            for (unsigned channels = 1; channels <= 2; ++channels)
                for (unsigned rate = 1; rate <= 2; ++rate)
                    check_durations(uneven, 4, splice, channels, rate, clocked);
    check_durations(fast, 4, 24, 2, 1, true);
    puts("variable: uneven/fast/long slices, between-edge timing, 24/96 tick grids, mono/stereo and 44.1/88.2 kHz passed");
    check_single_and_short();
    check_jump_reverse_and_suppression();
    check_selection_and_tempo();
    check_selection_policies_and_priorities();
    puts("variable: single/short slices, jump deadlines, reverse wrap, suppression, selection and live tempo passed");
    puts("variable: sequence/AMEN slice durations, retrigger/sequencer/stretch priority and one-shot transport passed");
    check_loss_and_restart(false);
    check_loss_and_restart(true);
    check_tap_mode(false);
    check_tap_mode(true);
    check_reset_during_stutter();
    puts("variable: clock loss stop/continue, immediate restart, TAP+MODE edges and reset during stutter passed");
}
