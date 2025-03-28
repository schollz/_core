// requires pcg_basic.h

pcg32_random_t rng;

int random_integer_in_range(int min, int max) {
  return (int)pcg32_boundedrand_r(&rng, max + 1 - min) + min;
}

void random_initialize() {
  pcg32_srandom_r(&rng, time_us_64() ^ (intptr_t)&printf, 54u);
}