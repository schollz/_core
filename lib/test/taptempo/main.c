#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#define LINUX_SYSTEM 1
#include "../../taptempo.h"

// Global variables
volatile bool key_pressed = false;
volatile bool exit_requested = false;
struct timeval last_time;

// Function to reset terminal settings
void reset_terminal_mode(struct termios *original) {
  tcsetattr(0, TCSANOW, original);
}

// Function to set terminal to raw mode
void set_conio_terminal_mode(struct termios *original) {
  struct termios new_term;
  tcgetattr(0, original);

  new_term = *original;
  cfmakeraw(&new_term);
  tcsetattr(0, TCSANOW, &new_term);
}

// Signal handler for SIGINT
void sigint_handler(int sig_num) {
  // Set the exit_requested flag
  exit_requested = true;
}

// Thread function to detect key press
void *key_press_detector(void *arg) {
  struct termios original;
  set_conio_terminal_mode(&original);

  int ch;
  // Detect key press
  while (!exit_requested) {
    ch = getchar();
    if (ch != EOF) {
      key_pressed = true;
      gettimeofday(&last_time, NULL);  // Update the time of last key press
      if (ch == 3) {                   // ASCII value for Ctrl+C is 3
        exit_requested = true;
      }
    }
  }

  reset_terminal_mode(&original);
  return NULL;
}

int main() {
  pthread_t thread_id;

  // Set up signal handler for SIGINT
  signal(SIGINT, sigint_handler);

  // Create a thread to detect key press
  if (pthread_create(&thread_id, NULL, key_press_detector, NULL) != 0) {
    perror("pthread_create");
    return EXIT_FAILURE;
  }

  TapTempo *taptempo = TapTempo_malloc();

  while (!exit_requested) {
    usleep(10);
    if (key_pressed) {
      fprintf(stderr, "\r%d bpm ", TapTempo_tap(taptempo));
      key_pressed = false;
    }
  }

  // Clean up
  pthread_join(thread_id, NULL);
  printf("Exiting program.\n");

  return EXIT_SUCCESS;
}