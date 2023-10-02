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
  // transfer_distortion(v * (1 << sf->distortion_level)));
  return v;
#endif
}