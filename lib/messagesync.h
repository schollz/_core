#ifndef LIB_MESSAGESYNC_H
#define LIB_MESSAGESYNC_H 1
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 300

typedef struct MessageSync {
  bool hasMessage;
  char buffer[BUFFER_SIZE];
  int length;
} MessageSync;

MessageSync *MessageSync_malloc() {
  MessageSync *self = (MessageSync *)malloc(sizeof(MessageSync));
  if (self != NULL) {
    self->hasMessage = false;
    self->length = 0;
    self->buffer[0] = '\0';  // Initialize buffer with null terminator
  }
  return self;
}

bool MessageSync_hasMessage(MessageSync *self) { return self->hasMessage; }

void MessageSync_free(MessageSync *self) { free(self); }

void MessageSync_clear(MessageSync *self) {
  self->length = 0;
  self->hasMessage = false;
}

void MessageSync_append(MessageSync *self, const char *text, ...) {
  if (self->hasMessage) {
    return;
  }
  int textLength = strlen(text);
  if (self->length + textLength < BUFFER_SIZE) {
    memcpy(self->buffer + self->length, text, textLength);
    self->length += textLength;
  }
}

void MessageSync_printf(MessageSync *self, const char *text, ...) {
  if (self->hasMessage) {
    return;
  }
  va_list args;
  va_start(args, text);
  int textLength = vsnprintf(NULL, 0, text, args);
  va_end(args);
  if (self->length + textLength < BUFFER_SIZE) {
    va_start(args, text);
    vsnprintf(self->buffer + self->length, textLength + 1, text, args);
    va_end(args);
    self->length += textLength;
  }
}

void MessageSync_print(MessageSync *self) {
  if (self->length > 0) {
    fwrite(self->buffer, sizeof(char), self->length, stdout);
#ifdef INCLUDE_MIDI
    send_buffer_as_sysex(self->buffer, self->length);
#endif
  }
}

void MessageSync_lockIfNotEmpty(MessageSync *self) {
  if (self->length > 0) {
    self->hasMessage = true;
  }
}

void MessageSync_lock(MessageSync *self) { self->hasMessage = true; }

#endif
