
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
