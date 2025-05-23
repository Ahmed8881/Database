#include "../include/input_handling.h"
#include <stdbool.h>
#include <ctype.h>

Input_Buffer *newInputBuffer() {
  Input_Buffer *buf = (Input_Buffer *)malloc(sizeof(Input_Buffer));
  buf->buffer = NULL;
  buf->buffer_length = buf->input_length = 0;
  buf->prompt_displayed = false; // Initialize prompt_displayed
  return buf;
}

void print_prompt() { printf("db > "); }

// Helper function to check if string is only whitespace
#ifdef UNUSED
static bool is_whitespace(const char *str) {
    while (*str != '\0') {
        if (!isspace(*str)) {
            return false;
        }
        str++;
    }
    return true;
}
#endif

void read_input(Input_Buffer *buf) {
  size_t buffer_size = 1024;
  if (buf->buffer == NULL) {
    buf->buffer = (char *)malloc(buffer_size);
    if (buf->buffer == NULL) {
      perror("Unable to allocate buffer");
      exit(EXIT_FAILURE);
    }
    buf->buffer_length = buffer_size;
  }

  // Only show prompt if it hasn't been displayed yet
  if (!buf->prompt_displayed) {
    print_prompt();
    buf->prompt_displayed = true; // Set the flag to true after displaying
  }

  size_t position = 0;
  int c;

  while (true) {
    c = fgetc(stdin);
    
    // Handle EOF (Ctrl+D) or error
    if (c == EOF) {
      if (position == 0) {
        printf("Exiting due to EOF\n");
        exit(EXIT_SUCCESS);
      } else {
        buf->buffer[position] = '\0';
        buf->input_length = position;
        break;
      }
    }
    
    // Break on newline
    if (c == '\n') {
      buf->buffer[position] = '\0';
      buf->input_length = position;
      break;
    } else {
      buf->buffer[position] = c;
    }
    position++;

    // Resize buffer if needed
    if (position >= buf->buffer_length - 1) {
      buf->buffer_length *= 2;
      buf->buffer = (char *)realloc(buf->buffer, buf->buffer_length);
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
