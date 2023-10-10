#include <stdio.h>
#include <stdlib.h>

// Define a function pointer.
typedef void (*callback_fn_int)(int);
typedef void (*callback_fn_int_int)(int, int);

// Define a callback function.
void callback_fn(int arg) {
  printf("Callback function called with argument %d\n", arg);
}

void callback_fn2(int arg1, int arg2) {
  printf("Callback function called with arguments %d, %d\n", arg1, arg2);
}

// Define a function that takes a callback function as an argument.
void call_callback(callback_fn_int fn, callback_fn_int_int fn2, int arg,
                   int arg2) {
  fn(arg);
  fn2(arg, arg2);
}

typedef struct Test {
  callback_fn_int fn;
  callback_fn_int fn2;
} Test;

// Main function.
int main() {
  // Call the call_callback function with the callback_fn callback function.
  call_callback(callback_fn, callback_fn2, 10, 11);

  Test *t = (Test *)malloc(sizeof(Test));
  t->fn = callback_fn;
  t->fn2 = NULL;
  if (t->fn2 != NULL) {
    t->fn2(3);
  }
  if (t->fn != NULL) {
    t->fn(32);
  }

  return 0;
}