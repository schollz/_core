
#define ALPHA_MAX 512

typedef struct FilterExp {
  int alpha;
  int filtered;
} FilterExp;

FilterExp *FilterExp_create(int alpha) {
  FilterExp *fe = (FilterExp *)malloc(sizeof(FilterExp));
  fe->filtered = 0;
  fe->alpha = alpha;
  return fe;
}

int FilterExp_update(FilterExp *fe, int x) {
  fe->filtered = fe->alpha * x + (ALPHA_MAX - fe->alpha) * fe->filtered;
  fe->filtered = fe->filtered / ALPHA_MAX;
  return fe->filtered;
}

typedef struct FilterExpUint32 {
  uint32_t alpha;
  uint32_t filtered;
} FilterExpUint32;

FilterExpUint32 *FilterExpUint32_create(uint32_t alpha) {
  FilterExpUint32 *fe = (FilterExpUint32 *)malloc(sizeof(FilterExpUint32));
  fe->filtered = 0;
  fe->alpha = alpha;
  return fe;
}

uint32_t FilterExpUint32_update(FilterExpUint32 *fe, uint32_t x) {
  fe->filtered = fe->alpha * x + (ALPHA_MAX - fe->alpha) * fe->filtered;
  fe->filtered = fe->filtered / ALPHA_MAX;
  return fe->filtered;
}
