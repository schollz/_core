// requires pcg_basic.h

pcg32_random_t rng_seeded;

int random_seeded_integer_in_range(int min, int max) {
  return (int)pcg32_boundedrand_r(&rng_seeded, max + 1 - min) + min;
}

void random_seeded_initialize(int64_t seed) {
  pcg32_srandom_r(&rng_seeded, seed, 54u);
}