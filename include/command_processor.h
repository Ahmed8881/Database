#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "input_handling.h"
#include "table.h"

typedef enum
{
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum
{
  PREPARE_SUCCESS,
  PREPARE_NEGATIVE_ID,
  PREPARE_STRING_TOO_LONG,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum
{
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL,
  EXECUTE_UNRECOGNIZED_STATEMENT,
  EXECUTE_DUPLICATE_KEY
} ExecuteResult;

typedef enum
{
  STATEMENT_INSERT,
  STATEMENT_SELECT,
  STATEMENT_SELECT_BY_ID,
  STATEMENT_UPDATE,
  STATEMENT_DELETE
} StatementType;

typedef struct
{
  StatementType type;
  Row row_to_insert;
  uint32_t id_to_select;
  uint32_t id_to_update;
  uint32_t id_to_delete;
} Statement;
/*** Printing start ***/
void print_constants();
void indent(uint32_t level);
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);
/*** Printing end ***/
MetaCommandResult do_meta_command(Input_Buffer *buf, Table *table);
PrepareResult prepare_insert(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);

#endif
