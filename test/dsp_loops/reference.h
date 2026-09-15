// Frozen process functions from commit 9cc7cac, for output/state equivalence.
// Copyright 2023-2025 Zack Scholl, GPLv3.0

void reference_delay(Delay *tapeDelay, int32_t *samples, unsigned int nr_samples,
                   uint8_t channel) {
  if (tapeDelay == NULL || !tapeDelay->on) {
    return;
  }

  for (unsigned int i = 0; i < nr_samples; i++) {
    // Smooth the delay time for gradual transitions
    tapeDelay->smoothed_delay_time +=
        0.01f * (tapeDelay->delay_time - tapeDelay->smoothed_delay_time);

    float fractional_read_index =
        (float)tapeDelay->write_index - tapeDelay->smoothed_delay_time;
    if (fractional_read_index < 0) {
      fractional_read_index += tapeDelay->buffer_size;
    }

    size_t base_read_index =
        (size_t)fractional_read_index % tapeDelay->buffer_size;
    size_t next_read_index = (base_read_index + 1) % tapeDelay->buffer_size;
    float frac = fractional_read_index - (size_t)fractional_read_index;

    // Read the delayed sample with interpolation
    int32_t delayed_sample =
        linear_interpolation(tapeDelay->buffer[base_read_index],
                             tapeDelay->buffer[next_read_index], frac);

    // Add feedback to the current sample and write it to the buffer
    int32_t input_sample = samples[i * 2 + channel];
    int32_t feedback_sample =
        q16_16_multiply(tapeDelay->feedback_fp, delayed_sample);
    int32_t processed_sample = add_and_softclip(input_sample, feedback_sample);

    tapeDelay->buffer[tapeDelay->write_index] = processed_sample;

    // Update write index
    tapeDelay->write_index =
        (tapeDelay->write_index + 1) % tapeDelay->buffer_size;

    // Store the processed sample back in the buffer
    samples[i * 2 + channel] = processed_sample;
    samples[i * 2 + 1] = add_and_softclip(samples[i * 2 + 1], feedback_sample);
  }
}

void reference_filter(ResonantFilter* rf,
                                                int32_t* samples,
                                                uint16_t num_samples,
                                                uint8_t channel) {
  if (rf->do_setFilterType && rf->passthrough) {
    ResonantFilter_setFilterType_(rf, rf->do_setFilterType_val);
    rf->do_setFilterType = false;
    if (rf->do_setFc) {
      ResonantFilter_setFc_(rf, rf->do_setFc_val);
    }
    ResonantFilter_reset(rf);
  } else if (rf->do_setFc) {
    ResonantFilter_setFc_(rf, rf->do_setFc_val);
    if (rf->do_setFilterType) {
      ResonantFilter_setFilterType_(rf, rf->do_setFilterType_val);
      rf->do_setFilterType = false;
    }
    ResonantFilter_reset(rf);
  }
  if (rf->passthrough && !rf->filter_was_on) {
    return;
  }
  int32_t y;
  for (uint16_t i = 0; i < num_samples; i++) {
    y = q16_16_multiply(rf->b0, samples[i * 2 + channel]) +
        q16_16_multiply(rf->b1, rf->x1_f) + q16_16_multiply(rf->b2, rf->x2_f) -
        q16_16_multiply(rf->a1, rf->y1_f) - q16_16_multiply(rf->a2, rf->y2_f);

    rf->x2_f = rf->x1_f;
    rf->x1_f = samples[i * 2 + channel];
    rf->y2_f = rf->y1_f;
    rf->y1_f = y;
    if (rf->passthrough && rf->filter_was_on) {
      // fade out the filter
      samples[i * 2 + channel] =
          q16_16_multiply(y, crossfade3_cos_out[i]) +
          q16_16_multiply(samples[i * 2 + channel], crossfade3_cos_in[i]);
    } else if (!rf->passthrough && !rf->filter_was_on) {
      // fade in the filter
      samples[i * 2 + channel] =
          q16_16_multiply(y, crossfade3_cos_in[i]) +
          q16_16_multiply(samples[i * 2 + channel], crossfade3_cos_out[i]);
    } else {
      samples[i * 2 + channel] = y;
    }
  }
  if (rf->passthrough && rf->filter_was_on) {
    rf->filter_was_on = false;
  } else if (!rf->passthrough && !rf->filter_was_on) {
    rf->filter_was_on = true;
  }
}
