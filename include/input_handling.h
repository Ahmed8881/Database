#ifndef INPUT_HANDLING_H
#define INPUT_HANDLING_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct {
  char *buffer;
  size_t buZffer_length;
  ssize_t input_length;
} Input_Buffer;

Input_Buffer *newInputBuffer();
void read_input(Input_Buffer *buf);
void free_input_buffer(Input_Buffer *buf);

#endif
