#define M_PI 3.14159265358979323846

// Define constants as needed

#define CLAMP(x, a, b) (x > a ? a : (x < b ? b : x))

#define qFactor (31)
#define scaleQ (powf(2.0, qFactor))

#define ACC_MAX ((int64_t)0x7FFFFFFFFF)
#define ACC_MIN ((int64_t)-0x8000000000)
#define ACC_REM ((uint64_t)0x3FFFFFFFU)

// Define IIR struct as needed
typedef struct {
  float Fc;
  float Q;
  float peakGain;
  float Fs;
  int32_t b[3];
  int32_t a[2];
  int32_t x[2];
  int32_t y[2];
  int32_t state_error;
} IIR;

// int main() {
//   // Usage example
//   IIR myFilter = create_IIR(lowpass, 1000.0f, 1.0f, 0.0f, 44100.0f);
//   int32_t signal = 1000;
//   IIR_filter(&myFilter, &signal);
//   return 0;
// }

void IIR_filter(IIR *filter, int32_t *s) {
  if (filter->Fc > 14000) {
    return;
  }
  int64_t accumulator = (int64_t)(filter->state_error);
  accumulator += (int64_t)(filter->b[0]) * (int64_t)(*s);
  accumulator += (int64_t)(filter->b[1]) * (int64_t)(filter->x[0]);
  accumulator += (int64_t)(filter->b[2]) * (int64_t)(filter->x[1]);
  accumulator += (int64_t)(filter->a[0]) * (int64_t)(filter->y[0]);
  accumulator += (int64_t)(filter->a[1]) * (int64_t)(filter->y[1]);

  filter->state_error = accumulator & ACC_REM;
  int32_t out = (int32_t)(accumulator >> (int64_t)(qFactor));

  filter->x[1] = filter->x[0];
  filter->y[1] = filter->y[0];

  filter->x[0] = (*s);
  filter->y[0] = out;

  *s = out;
}

void IIR_set_fc(IIR *filter, float Fc) {
  filter->Fc = Fc;
  float a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0, norm = 0;
  // float V = powf(10, fabsf(peakGain) / 20);
  float K = tanf(M_PI * Fc / filter->Fs);
  // default is lowpass
  norm = 1 / (1 + K / filter->Q + K * K);
  a0 = K * K * norm;
  a1 = 2 * a0;
  a2 = a0;
  b1 = 2 * (K * K - 1) * norm;
  b2 = (1 - K / filter->Q + K * K) * norm;
  filter->b[0] = (int32_t)(a0 * scaleQ);
  filter->b[1] = (int32_t)(a1 * scaleQ);
  filter->b[2] = (int32_t)(a2 * scaleQ);
  filter->a[0] = (int32_t)(-b1 * scaleQ);
  filter->a[1] = (int32_t)(-b2 * scaleQ);

  // float w0 = 2 * M_PI * (Fc / filter->Fs);
  // float sinW = sin(w0);
  // float cosW = cos(w0);
  // float alpha = sinW / (2 * filter->Q);
  // float a0 = 1 + alpha;
  // float a1 = -2 * cosW;
  // float a2 = 1 - alpha;
  // float b0 = (1 - cosW) / 2;
  // float b1 = b0 * 2;
  // float b2 = b0;
  // b0 = b0 / a0;
  // b1 = b1 / a0;
  // b2 = b2 / a0;
  // a1 = a1 / a0;
  // a2 = a2 / a0;
  // filter->b[0] = (int32_t)(b0 * scaleQ);
  // filter->b[1] = (int32_t)(b1 * scaleQ);
  // filter->b[2] = (int32_t)(b2 * scaleQ);
  // filter->a[0] = (int32_t)(-a1 * scaleQ);
  // filter->a[1] = (int32_t)(-a2 * scaleQ);

  filter->x[0] = 0;
  filter->x[1] = 0;
  filter->y[0] = 0;
  filter->y[1] = 0;
  filter->state_error = 0;
}

IIR *IIR_new(float Fc, float Q, float peakGain, float Fs) {
  IIR *filter;

  filter = malloc(sizeof(IIR));
  filter->Q = Q;
  filter->peakGain = peakGain;
  filter->Fs = Fs;

  IIR_set_fc(filter, Fc);

  // printf("norm: %2.5f\n", norm);
  // printf("a0: %2.5f\n", a0);
  // printf("a1: %2.5f\n", a1);
  // printf("a2: %2.5f\n", a2);
  // printf("b1: %2.5f\n", b1);
  // printf("b2: %2.5f\n", b2);
  // printf("K: %2.5f\n", K);
  // printf("scaleQ: %2.5f\n", scaleQ);
  // printf("(a0 * scaleQ): %2.5f\n", (a0 * scaleQ));
  // printf("int32_t(a0 * scaleQ): %d\n", (int32_t)(a0 * scaleQ));
  // printf("filter->b[0]: %d\n", filter->b[0]);
  // printf("filter->b[1]: %d\n", filter->b[1]);
  // printf("filter->b[2]: %d\n", filter->b[2]);
  // printf("filter->a[0]: %d\n", filter->a[0]);
  // printf("filter->a[1]: %d\n", filter->a[1]);

  return filter;
}
