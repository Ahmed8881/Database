#include "include/command_processor.h"
#include "include/input_handling.h"
#include "include/table.h"

int main() {
  system("clear");
  Table *table = new_table();
  Input_Buffer *input_buf = newInputBuffer();
  while (1) {
    read_input(input_buf);
    if (input_buf->buffer[0] == '.') {
      switch (do_meta_command(input_buf)) {
      case META_COMMAND_SUCCESS:
        continue;
      case META_COMMAND_UNRECOGNIZED_COMMAND:
        printf("Unrecognized command %s\n", input_buf->buffer);
        continue;
      }
    } else {
      Statement statement;
      switch (prepare_statement(input_buf, &statement)) {
      case PREPARE_SUCCESS:
        break;
      case PREPARE_SYNTAX_ERROR:
        printf("Syntax error. Could not parse statement.\n");
        continue;
      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized keyword at the start of '%s'.\n",
               input_buf->buffer);
        continue;
      }
      switch (execute_statement(&statement, table)) {
      case EXECUTE_SUCCESS:
        printf("Executed.\n");
        break;
      case EXECUTE_TABLE_FULL:
        printf("Error: Table full.\n");
        break;
      case EXECUTE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized statement at '%s'.\n", input_buf->buffer);
      }
    }
  }
}
