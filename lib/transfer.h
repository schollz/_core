int16_t transfer_fn(int16_t v) {
#ifndef INCLUDE_TRANSFER
  return v;
#endif
#ifdef INCLUDE_TRANSFER
  if (sf->saturate_wet > 0) {
    v = selectx(sf->saturate_wet, v, transfer_doublesine(v));
  }
  if (sf->distortion_wet && sf->distortion_level > 0) {
    v = selectx(sf->distortion_wet, v,
                transfer_distortion(v * sf->distortion_level));
  }
  if (sf->wavefold > 0 && v != 0) {
    int32_t x2 = v * (sf->wavefold + 100) / 100;
    if (x2 > 32767) {
      v = 32767 - (x2 - 32767);
    } else if (x2 < -32767) {
      v = -32767 - (x2 + 32767);
    }
  }
  return v;
#endif
}