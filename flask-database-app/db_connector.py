import subprocess
import json
import os
import tempfile
import time
from typing import List, Dict, Any, Optional, Union

class DatabaseConnector:
    """Interface with the C database application"""
    
    def __init__(self, db_path: str):
        """Initialize connector with path to database"""
        self.db_path = db_path
        self.db_binary = os.path.join(db_path, 'bin', 'db-project')
        self.database_name = "myusers"
        self.table_name = "users"
        
        # Create the database and table if they don't exist
        self._setup_database()
    
    def _setup_database(self) -> None:
        """Set up the database and table if they don't exist"""
        # Create database and table in one go to ensure they exist
        create_commands = [
            f"CREATE DATABASE {self.database_name}",
            f"USE DATABASE {self.database_name}",
            f"CREATE TABLE {self.table_name} (id INT, name STRING(100), email STRING(255))"
        ]
        self.run_command("\n".join(create_commands))
    
    
    def get_tables(self) -> List[str]:
        """Get a list of available tables"""
        output = self.run_command("SHOW TABLES")
        tables = []
        
        # Parse the output to extract table names
        lines = output.strip().split('\n')
        for line in lines:
            if "Table:" in line:
                table_name = line.split("Table:")[1].strip()
                tables.append(table_name)
        
        if not tables:
            # If parsing failed, at least return our known table
            return [self.table_name]
            
        return tables
    
    def get_records(self, table_name: str) -> List[Dict[str, Any]]:
        """Get all records from a table"""
        output = self.run_command(f"USE TABLE {table_name}\nSELECT * FROM {table_name}")
        records = []
        
        # Better parsing of the output
        print(f"Raw output for records: {output}")
        
        lines = output.strip().split('\n')
        header_found = False
        columns = []
        in_data_section = False
        
        for line in lines:
            # Debug
            print(f"Processing line: '{line}'")
            
            # Skip empty lines and db prompt lines
            if not line.strip() or line.strip().startswith('db >'):
                continue
            
            # Look for header row with column names
            if not header_found and ('id' in line.lower() and 'name' in line.lower()):
                header_found = True
                columns = []
                # Extract column names, handle different formats
                parts = line.split('|')
                for part in parts:
                    col = part.strip()
                    if col:
                        columns.append(col.lower())
                print(f"Found columns: {columns}")
                continue
            
            # Process data rows - must contain pipe character and not be a command response
            if header_found and '|' in line and not any(x in line for x in ['Executed', 'Error', 'Table:']):
                values = [val.strip() for val in line.split('|') if val]
                
                if len(values) >= len(columns) and len(columns) > 0:
                    record = {}
                    for i, column in enumerate(columns):
                        if i < len(values):
                            value = values[i]
                            # Convert ID to integer if possible
                            if column == 'id' and value.isdigit():
                                record[column] = int(value)
                            else:
                                record[column] = value
                    
                    print(f"Found record: {record}")
                    records.append(record)
        
        # If all else fails, try loading the data_cache.json file
        if not records:
            try:
                cache_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data_cache.json')
                if os.path.exists(cache_file):
                    with open(cache_file, 'r') as f:
                        cache_data = json.load(f)
                        if 'records' in cache_data and table_name in cache_data['records']:
                            return cache_data['records'][table_name]
            except Exception as e:
                print(f"Error reading cache file: {e}")
        
        return records
    
    def get_record_by_id(self, table_name: str, record_id: int) -> Optional[Dict[str, Any]]:
        """Get a specific record by ID"""
        output = self.run_command(f"USE TABLE {table_name}\nSELECT * FROM {table_name} WHERE id = {record_id}")
        
        # Parse the output to extract the record
        lines = output.strip().split('\n')
        header_found = False
        columns = []
        record = None
        
        for line in lines:
            if "id" in line.lower() and "name" in line.lower() and "email" in line.lower():
                # This is likely the header line
                header_found = True
                columns = [col.strip() for col in line.split('|') if col.strip()]
            elif header_found and '|' in line and not line.startswith('db >'):
                # This is a data row
                values = [val.strip() for val in line.split('|') if val.strip()]
                if len(values) >= len(columns):
                    record = {}
                    for i, column in enumerate(columns):
                        column_lower = column.lower()
                        if column_lower == 'id' and values[i].isdigit():
                            record[column_lower] = int(values[i])
                        else:
                            record[column_lower] = values[i]
                    break  # We only need the first matching record
        
        return record
    
    def insert_record(self, table_name: str, record: Dict[str, Any]) -> bool:
        """Insert a new record into a table"""
        try:
            values_str = f"{record['id']}, \"{record['name']}\", \"{record['email']}\""
            output = self.run_command(f"USE TABLE {table_name}\nINSERT INTO {table_name} VALUES ({values_str})")
            
            # Check if the command was successful
            if "Executed" in output:
                return True
            return False
        except Exception as e:
            print(f"Error inserting record: {e}")
            return False
    
    def update_record(self, table_name: str, record_id: int, record: Dict[str, Any]) -> bool:
        """Update an existing record"""
        try:
            # Check if record exists first
            existing_record = self.get_record_by_id(table_name, record_id)
            if not existing_record:
                return False
            
            # Execute commands to update each field
            commands = []
            commands.append(f"USE TABLE {table_name}")
            
            if 'name' in record:
                commands.append(f"UPDATE {table_name} SET name = \"{record['name']}\" WHERE id = {record_id}")
            
            if 'email' in record:
                commands.append(f"UPDATE {table_name} SET email = \"{record['email']}\" WHERE id = {record_id}")
            
            # Run the commands
            output = self.run_command("\n".join(commands))
            
            # Check if the command was successful
            return "Executed" in output
        except Exception as e:
            print(f"Error updating record: {e}")
            return False
    
    def delete_record(self, table_name: str, record_id: int) -> bool:
        """Delete a record"""
        try:
            output = self.run_command(f"USE TABLE {table_name}\nDELETE FROM {table_name} WHERE id = {record_id}")
            
            # Check if the command was successful
            return "Executed" in output
        except Exception as e:
            print(f"Error deleting record: {e}")
            return False
    
    def get_next_id(self, table_name: str) -> int:
        """Get the next available ID for a table"""
        records = self.get_records(table_name)
        if not records:
            return 1
        
        # Find the maximum ID
        max_id = 0
        for record in records:
            if 'id' in record and isinstance(record['id'], int):
                max_id = max(max_id, record['id'])
        
        return max_id + 1
    
    def run_command(self, command: str) -> str:
        """Run a command on the database binary and return output"""
        try:
            # Create a temporary script file with the command
            with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
                # First make sure we're creating and using the database
                f.write(f"CREATE DATABASE {self.database_name}\n")
                f.write(f"USE DATABASE {self.database_name}\n")
                
                # Then write the actual command
                f.write(command + "\n.exit\n")
                temp_file = f.name
            
            # Since we're already in WSL, directly use the binary
            db_binary_path = self.db_binary
            if os.path.exists('/mnt/c'):
                # We're in WSL, convert Windows path to WSL path
                db_binary_path = self._convert_to_wsl_path(self.db_binary)
            
            print(f"Running command: {command}")
            print(f"Using binary: {db_binary_path}")
            
            # Make sure the binary is executable
            if os.path.exists(db_binary_path):
                os.chmod(db_binary_path, 0o755)
            
            # Execute directly (not using wsl command)
            process = subprocess.Popen(
                [db_binary_path],
                stdin=open(temp_file, 'r'),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            stdout, stderr = process.communicate()
            
            # Clean up
            os.unlink(temp_file)
            
            if stderr:
                print(f"Error executing command: {stderr}")
            
            print(f"Command output: {stdout}")
            return stdout
        except Exception as e:
            print(f"Error running command: {e}")
            return f"Error: {str(e)}"

    def _convert_to_wsl_path(self, windows_path: str) -> str:
        """Convert a Windows path to a WSL path"""
        # Remove drive letter and convert backslashes to forward slashes
        path = windows_path.replace('\\', '/')
        
        # If it has a drive letter like C:, convert to /mnt/c
        if ':' in path:
            drive, rest = path.split(':', 1)
            return f"/mnt/{drive.lower()}{rest}"
        return path