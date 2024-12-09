#include "../include/input_handling.h"
#include <stdbool.h>

Input_Buffer *newInputBuffer() {
  Input_Buffer *buf = (Input_Buffer *)malloc(sizeof(Input_Buffer));
  buf->buffer = NULL;
  buf->buffer_length = buf->input_length = 0;
  return buf;
}

void print_prompt() { printf("db > "); }

void read_input(Input_Buffer *buf) {
  size_t buffer_size = 1024;
  buf->buffer = (char *)malloc(buffer_size);
  if (buf->buffer == NULL) {
    perror("Unable to allocate buffer");
    exit(EXIT_FAILURE);
  }

  print_prompt();

  size_t position = 0;
  int c;

  while (true) {
    c = fgetc(stdin);
    if (c == '\n' || c == EOF) {
      buf->buffer[position] = '\0';
      buf->input_length = position;
      break;
    } else {
      buf->buffer[position] = c;
    }
    position++;

    if (position >= buffer_size) {
      // double the buffer size
      buffer_size *= 2;
      buf->buffer = (char *)realloc(buf->buffer, buffer_size);
      if (buf->buffer == NULL) {
        perror("Unable to reallocate buffer");
        exit(EXIT_FAILURE);
      }
    }
  }
}

void free_input_buffer(Input_Buffer *buf) {
  free(buf->buffer);
  free(buf);
}
