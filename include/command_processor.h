#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "input_handling.h"
#include "table.h"

typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum { EXECUTE_SUCCESS, EXECUTE_TABLE_FULL, EXECUTE_UNRECOGNIZED_STATEMENT } ExecuteResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
  StatementType type;
  Row row_to_insert;
} Statement;

MetaCommandResult do_meta_command(Input_Buffer *buf);
PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);

#endif