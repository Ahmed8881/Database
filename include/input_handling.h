#ifndef INPUT_HANDLING_H
#define INPUT_HANDLING_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>  // Add this include for bool type

typedef struct {
  char *buffer;
  size_t buffer_length;
  ssize_t input_length;
  bool prompt_displayed;  // Flag to indicate if prompt was already displayed
} Input_Buffer;

Input_Buffer *newInputBuffer();
void read_input(Input_Buffer *buf);
void free_input_buffer(Input_Buffer *buf);

#endif
