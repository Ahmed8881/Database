#ifndef INPUT_HANDLING_H
#define INPUT_HANDLING_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#define INPUT_BUFFER_SIZE 4096

typedef struct {
  char *buffer;
  size_t buffer_length;
  ssize_t input_length;
} Input_Buffer;

Input_Buffer *newInputBuffer();
void read_input(Input_Buffer *buf);
void free_input_buffer(Input_Buffer *buf);

#endif
