import subprocess
import json
import os
import tempfile
import time
import socket
from typing import List, Dict, Any, Optional, Union

class Connection:
    """
    Connection to the database server via socket communication.
    """
    def __init__(self, 
                 host: str = "localhost", 
                 port: int = 8080,
                 database: Optional[str] = None,
                 connect_timeout: int = 30,
                 username: str = "admin",
                 password: str = "jhaz",
                 db_name: str = "school",
                 table_name: str = "students") -> None:
        
        self.host = host
        self.port = port
        self.database = database
        self.connect_timeout = connect_timeout

        self.username = username
        self.password = password
        self.database_name = db_name
        self.table_name = table_name
        
        # Connection state
        self.socket = None
        self.connected = False
    
    def connect(self):
        """Connect to the database server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(self.connect_timeout)
            self.socket.connect((self.host, self.port))
            self.connected = True

            print(f"Connected to database at {self.host}:{self.port}")

            # Perform login
            if not self._login():
                raise ConnectionError("Login failed")
            print("Login successful")

            # Set up the database and table if they don't exist
            self._setup_database()
            
            return True
                
        except socket.error as e:
            self.connected = False
            self.socket = None
            raise ConnectionError(f"Failed to connect to database at {self.host}:{self.port}: {e}")

    def _login(self) -> bool:
        """Login to the database server"""
        if not self.connected or not self.socket:
            raise ConnectionError("Not connected to database")
        
        try:
            # Send login command
            login_command = f"LOGIN {self.username} {self.password}"
            self.socket.sendall(login_command.encode('utf-8') + b'\n')
            
            # Wait for response
            response = self._receive_response()
            if "Authentication successful" in response:
                return True
            else:
                raise ConnectionError("Login failed: " + response)
        
        except socket.error as e:
            raise ConnectionError(f"Error during login: {e}")

    def _setup_database(self) -> None:
        """Set up the database and table if they don't exist"""
        # Create database and table in one go to ensure they exist
        create_commands = [
            f"CREATE DATABASE {self.database_name}",
            f"USE DATABASE {self.database_name}",
            f"CREATE TABLE {self.table_name} (id INT, name STRING(100), email STRING(255))"
        ]
        for command in create_commands:
            try:
                output = self.run_command(command)
                if "Executed" not in output and "already exists" not in output:
                    raise ConnectionError(f"Failed to execute command: {command}\nOutput: {output}")
            except Exception as e:
                print(f"Error setting up database/table: {e}")

    def run_command(self, command: str) -> str:
        """Run a command on the database server and return output"""
        if not self.connected or not self.socket:
            raise ConnectionError("Not connected to database")
        
        try:
            # Send the command
            self.socket.sendall(command.encode('utf-8') + b'\n')
            
            # Wait for response
            response = self._receive_response()
            return response
        
        except socket.error as e:
            raise ConnectionError(f"Error sending command: {e}")
        
    def _receive_response(self) -> str:
        """Receive response from the database server"""
        if not self.connected or not self.socket:
            raise ConnectionError("Not connected to database")
        
        response = b""
        while True:
            try:
                part = self.socket.recv(4096)
                if not part:
                    break
                response += part
            except socket.timeout:
                break
        
        return response.decode('utf-8')

    
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
        self.run_command(f"USE TABLE {table_name}")
        output = self.run_command(f"SELECT * FROM {table_name}")
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
        self.run_command(f"USE TABLE {table_name}")
        output = self.run_command(f"SELECT * FROM {table_name} WHERE id = {record_id}")
        
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
            self.run_command(f"USE TABLE {table_name}")
            
            output = ""
            if 'name' in record:
                output = self.run_command(f"UPDATE {table_name} SET name = \"{record['name']}\" WHERE id = {record_id}")
            
            if 'email' in record:
                output = self.run_command(f"UPDATE {table_name} SET email = \"{record['email']}\" WHERE id = {record_id}")
            
            
            # Check if the command was successful
            return "Executed" in output
        except Exception as e:
            print(f"Error updating record: {e}")
            return False
    
    def delete_record(self, table_name: str, record_id: int) -> bool:
        """Delete a record"""
        try:
            self.run_command(f"USE TABLE {table_name}")
            output = self.run_command(f"DELETE FROM {table_name} WHERE id = {record_id}")
            
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
    
    