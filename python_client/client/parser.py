import re
import json
from typing import Dict, List, Any, Optional, Union, Tuple

class SQLParser:
    """
    Parse SQL queries into structured JSON objects.
    Follows the command patterns from the existing C implementation.
    """
    def __init__(self, transaction_id: Optional[int] = None):
        self.transaction_id = transaction_id
        
    def parse(self, query: str) -> Dict[str, Any]:
        """Parse SQL query and convert to JSON structure"""
        query = query.strip()
        
        # Normalize spaces
        query = re.sub(r'\s+', ' ', query)
        
        # Determine command type (first word)
        first_word = query.split(' ')[0].lower()
        
        # Dispatch to appropriate parser method
        if first_word == 'select':
            return self._parse_select(query)
        elif first_word == 'insert':
            return self._parse_insert(query)
        elif first_word == 'update':
            return self._parse_update(query)
        elif first_word == 'delete':
            return self._parse_delete(query)
        elif first_word == 'create' and 'table' in query.lower():
            return self._parse_create_table(query)
        elif first_word == 'create' and 'index' in query.lower():
            return self._parse_create_index(query)
        elif first_word == 'create' and 'database' in query.lower():
            return self._parse_create_database(query)
        elif first_word == 'use' and 'table' in query.lower():
            return self._parse_use_table(query)
        elif first_word == 'use' and 'database' in query.lower():
            return self._parse_use_database(query)
        elif first_word == 'show' and 'tables' in query.lower():
            return self._parse_show_tables(query)
        elif first_word == 'show' and 'indexes' in query.lower():
            return self._parse_show_indexes(query)
        else:
            raise ValueError(f"Unsupported SQL command: {query}")
    
    def _parse_select(self, query: str) -> Dict[str, Any]:
        """
        Parse SELECT statements.
        Example: SELECT name, age FROM users WHERE id = 5
        """
        # Basic validation
        if not re.search(r'SELECT\s+.+\s+FROM\s+\w+', query, re.IGNORECASE):
            raise ValueError("Invalid SELECT query syntax")
        
        result = {
            "command": "select",
            "transaction_id": self.transaction_id
        }
        
        # Extract FROM clause and table name
        from_match = re.search(r'FROM\s+(\w+)', query, re.IGNORECASE)
        if from_match:
            result["table"] = from_match.group(1)
        else:
            raise ValueError("Missing FROM clause in SELECT query")
        
        # Extract column list
        columns_part = query[6:query.lower().find('from')].strip()
        if columns_part == '*':
            result["columns"] = []  # Empty list indicates all columns
        else:
            columns = [col.strip() for col in columns_part.split(',')]
            result["columns"] = columns
        
        # Check for WHERE clause
        where_match = re.search(r'WHERE\s+(.+)$', query, re.IGNORECASE)
        if where_match:
            where_clause = where_match.group(1).strip()
            # Parse condition (support = operator for now)
            condition_match = re.search(r'(\w+)\s*([=<>]+)\s*([\w\.\'\"]+)', where_clause)
            if condition_match:
                column, operator, value = condition_match.groups()
                
                # Try to convert value to appropriate type
                if value.isdigit():
                    value = int(value)
                elif value.lower() in ('true', 'false'):
                    value = value.lower() == 'true'
                elif value.startswith(("'", '"')) and value.endswith(("'", '"')):
                    value = value[1:-1]  # Remove quotes
                
                result["where"] = {
                    "column": column,
                    "operator": operator,
                    "value": value
                }
        
        return result
    
    def _parse_insert(self, query: str) -> Dict[str, Any]:
        """
        Parse INSERT statements.
        Example: INSERT INTO users VALUES (1, "John", "john@example.com")
        """
        # Match the INSERT INTO pattern
        match = re.search(r'INSERT\s+INTO\s+(\w+)\s+VALUES\s*\((.*)\)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid INSERT query syntax")
        
        table_name = match.group(1)
        values_str = match.group(2)
        
        # Parse values, handling quotes and commas
        values = []
        current_val = ""
        in_quotes = False
        quote_char = None
        
        for char in values_str:
            if char in ['"', "'"]:
                if not in_quotes:
                    in_quotes = True
                    quote_char = char
                    current_val += char
                elif char == quote_char:
                    in_quotes = False
                    quote_char = None
                    current_val += char
                else:
                    current_val += char
            elif char == ',' and not in_quotes:
                values.append(current_val.strip())
                current_val = ""
            else:
                current_val += char
        
        # Add the last value
        if current_val:
            values.append(current_val.strip())
        
        # Convert values to appropriate types
        processed_values = []
        for val in values:
            if val.isdigit():
                processed_values.append(int(val))
            elif val.lower() in ('true', 'false'):
                processed_values.append(val.lower() == 'true')
            elif (val.startswith('"') and val.endswith('"')) or (val.startswith("'") and val.endswith("'")):
                processed_values.append(val[1:-1])  # Remove quotes
            else:
                processed_values.append(val)
        
        return {
            "command": "insert",
            "table": table_name,
            "values": processed_values,
            "transaction_id": self.transaction_id
        }
    
    def _parse_update(self, query: str) -> Dict[str, Any]:
        """
        Parse UPDATE statements.
        Example: UPDATE users SET name = "Jane" WHERE id = 1
        """
        # Match the UPDATE pattern
        match = re.search(r'UPDATE\s+(\w+)\s+SET\s+(\w+)\s*=\s*([^WHERE]+)(WHERE\s+.+)?', 
                          query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid UPDATE query syntax")
        
        table_name = match.group(1)
        column_name = match.group(2)
        value = match.group(3).strip()
        where_clause = match.group(4) if match.group(4) else None
        
        # Process value - remove quotes if present
        if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
            value = value[1:-1]
        elif value.isdigit():
            value = int(value)
        elif value.lower() in ('true', 'false'):
            value = value.lower() == 'true'
        
        result = {
            "command": "update",
            "table": table_name,
            "column": column_name,
            "value": value,
            "transaction_id": self.transaction_id
        }
        
        # Process WHERE clause if present
        if where_clause:
            # Parse condition (support = operator for now)
            condition_match = re.search(r'WHERE\s+(\w+)\s*([=<>]+)\s*([\w\.\'\"]+)', where_clause, re.IGNORECASE)
            if condition_match:
                w_column, w_operator, w_value = condition_match.groups()
                
                # Process the value
                if w_value.isdigit():
                    w_value = int(w_value)
                elif w_value.lower() in ('true', 'false'):
                    w_value = w_value.lower() == 'true'
                elif (w_value.startswith('"') and w_value.endswith('"')) or (w_value.startswith("'") and w_value.endswith("'")):
                    w_value = w_value[1:-1]  # Remove quotes
                
                result["where"] = {
                    "column": w_column,
                    "operator": w_operator,
                    "value": w_value
                }
        
        return result
    
    def _parse_delete(self, query: str) -> Dict[str, Any]:
        """
        Parse DELETE statements.
        Example: DELETE FROM users WHERE id = 1
        """
        # Match the DELETE pattern
        match = re.search(r'DELETE\s+FROM\s+(\w+)(\s+WHERE\s+.+)?', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid DELETE query syntax")
        
        table_name = match.group(1)
        where_clause = match.group(2) if match.group(2) else None
        
        result = {
            "command": "delete",
            "table": table_name,
            "transaction_id": self.transaction_id
        }
        
        # Process WHERE clause if present
        if where_clause:
            # Parse condition (support = operator for now)
            condition_match = re.search(r'WHERE\s+(\w+)\s*([=<>]+)\s*([\w\.\'\"]+)', where_clause, re.IGNORECASE)
            if condition_match:
                column, operator, value = condition_match.groups()
                
                # Process the value
                if value.isdigit():
                    value = int(value)
                elif value.lower() in ('true', 'false'):
                    value = value.lower() == 'true'
                elif (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
                    value = value[1:-1]  # Remove quotes
                
                result["where"] = {
                    "column": column,
                    "operator": operator,
                    "value": value
                }
        
        return result
    
    def _parse_create_table(self, query: str) -> Dict[str, Any]:
        """
        Parse CREATE TABLE statements.
        Example: CREATE TABLE users (id INT, name STRING, email STRING)
        """
        # Match the CREATE TABLE pattern
        match = re.search(r'CREATE\s+TABLE\s+(\w+)\s*\((.*)\)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid CREATE TABLE syntax")
        
        table_name = match.group(1)
        columns_str = match.group(2)
        
        # Parse columns
        columns = []
        for column_def in [col.strip() for col in columns_str.split(',')]:
            parts = column_def.split()
            if len(parts) < 2:
                raise ValueError(f"Invalid column definition: {column_def}")
            
            col_name = parts[0]
            col_type = parts[1].upper()
            
            # Check for size in type (e.g., STRING(50))
            size_match = re.search(r'(\w+)\((\d+)\)', col_type)
            if size_match:
                col_type = size_match.group(1)
                size = int(size_match.group(2))
                columns.append({
                    "name": col_name,
                    "type": col_type,
                    "size": size
                })
            else:
                columns.append({
                    "name": col_name,
                    "type": col_type
                })
        
        return {
            "command": "create_table",
            "table": table_name,
            "columns": columns,
            "transaction_id": self.transaction_id
        }
    
    def _parse_create_index(self, query: str) -> Dict[str, Any]:
        """
        Parse CREATE INDEX statements.
        Example: CREATE INDEX idx_name ON users (name)
        """
        # Match the CREATE INDEX pattern
        match = re.search(r'CREATE\s+INDEX\s+(\w+)\s+ON\s+(\w+)\s*\((\w+)\)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid CREATE INDEX syntax")
        
        index_name = match.group(1)
        table_name = match.group(2)
        column_name = match.group(3)
        
        return {
            "command": "create_index",
            "index": index_name,
            "table": table_name,
            "column": column_name,
            "transaction_id": self.transaction_id
        }
    
    def _parse_create_database(self, query: str) -> Dict[str, Any]:
        """
        Parse CREATE DATABASE statements.
        Example: CREATE DATABASE mydatabase
        """
        # Match the CREATE DATABASE pattern
        match = re.search(r'CREATE\s+DATABASE\s+(\w+)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid CREATE DATABASE syntax")
        
        database_name = match.group(1)
        
        return {
            "command": "create_database",
            "database": database_name,
            "transaction_id": self.transaction_id
        }
    
    def _parse_use_table(self, query: str) -> Dict[str, Any]:
        """
        Parse USE TABLE statements.
        Example: USE TABLE users
        """
        # Match the USE TABLE pattern
        match = re.search(r'USE\s+TABLE\s+(\w+)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid USE TABLE syntax")
        
        table_name = match.group(1)
        
        return {
            "command": "use_table",
            "table": table_name,
            "transaction_id": self.transaction_id
        }
    
    def _parse_use_database(self, query: str) -> Dict[str, Any]:
        """
        Parse USE DATABASE statements.
        Example: USE DATABASE mydatabase
        """
        # Match the USE DATABASE pattern
        match = re.search(r'USE\s+DATABASE\s+(\w+)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid USE DATABASE syntax")
        
        database_name = match.group(1)
        
        return {
            "command": "use_database",
            "database": database_name,
            "transaction_id": self.transaction_id
        }
    
    def _parse_show_tables(self, query: str) -> Dict[str, Any]:
        """
        Parse SHOW TABLES statements.
        Example: SHOW TABLES
        """
        # Simple validation
        if not re.match(r'SHOW\s+TABLES', query, re.IGNORECASE):
            raise ValueError("Invalid SHOW TABLES syntax")
        
        return {
            "command": "show_tables",
            "transaction_id": self.transaction_id
        }
    
    def _parse_show_indexes(self, query: str) -> Dict[str, Any]:
        """
        Parse SHOW INDEXES statements.
        Example: SHOW INDEXES FROM users
        """
        # Match the SHOW INDEXES pattern
        match = re.search(r'SHOW\s+INDEXES\s+FROM\s+(\w+)', query, re.IGNORECASE)
        if not match:
            raise ValueError("Invalid SHOW INDEXES syntax")
        
        table_name = match.group(1)
        
        return {
            "command": "show_indexes",
            "table": table_name,
            "transaction_id": self.transaction_id
        }