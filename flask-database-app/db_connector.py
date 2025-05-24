import subprocess
import json
import os
import tempfile
from typing import List, Dict, Any, Optional, Union

class DatabaseConnector:
    """Interface with the C database application"""
    
    def __init__(self, db_path: str):
        """Initialize connector with path to database"""
        self.db_path = db_path
        self.db_binary = os.path.join(db_path, 'bin/db-project')
        self.json_cache_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data_cache.json')
        
        # Initialize cache if it doesn't exist
        if not os.path.exists(self.json_cache_file):
            self._initialize_cache()
    
    def _initialize_cache(self) -> None:
        """Initialize the JSON cache file with basic structure"""
        cache_data = {
            'databases': ['school', 'company'],
            'tables': {
                'users': {
                    'database': 'company',
                    'columns': [
                        {'name': 'id', 'type': 'INT'},
                        {'name': 'name', 'type': 'STRING'},
                        {'name': 'email', 'type': 'STRING'}
                    ]
                },
                'students': {
                    'database': 'school',
                    'columns': [
                        {'name': 'id', 'type': 'INT'},
                        {'name': 'name', 'type': 'STRING'},
                        {'name': 'father_name', 'type': 'STRING'},
                        {'name': 'gpa', 'type': 'FLOAT'},
                        {'name': 'age', 'type': 'INT'},
                        {'name': 'gender', 'type': 'STRING'}
                    ]
                }
            },
            'records': {
                'users': [
                    {'id': 1, 'name': 'John Doe', 'email': 'john@example.com'},
                    {'id': 2, 'name': 'Jane Smith', 'email': 'jane@example.com'}
                ],
                'students': [
                    {'id': 1, 'name': 'John Doe', 'father_name': 'Richard Roe', 'gpa': 3.5, 'age': 20, 'gender': 'M'},
                    {'id': 2, 'name': 'Jane Smith', 'father_name': 'John Smith', 'gpa': 3.8, 'age': 22, 'gender': 'F'},
                    {'id': 3, 'name': 'Alice Johnson', 'father_name': 'Robert Johnson', 'gpa': 3.2, 'age': 19, 'gender': 'F'}
                ]
            }
        }
        
        with open(self.json_cache_file, 'w') as f:
            json.dump(cache_data, f, indent=4)
    
    def _load_cache(self) -> Dict[str, Any]:
        """Load the current cache data"""
        with open(self.json_cache_file, 'r') as f:
            return json.load(f)
    
    def _save_cache(self, data: Dict[str, Any]) -> None:
        """Save data to the cache file"""
        with open(self.json_cache_file, 'w') as f:
            json.dump(data, f, indent=4)
    
    def run_command(self, command: str) -> str:
        """Run a command on the database binary and return output"""
        try:
            # Create a temporary script file with the command
            with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
                f.write(command + '\n.exit\n')
                temp_file = f.name
            
            # Run the command using the database binary
            process = subprocess.Popen(
                [self.db_binary],
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
            
            return stdout
        except Exception as e:
            print(f"Error running command: {e}")
            return ""
    
    def get_tables(self) -> List[str]:
        """Get a list of available tables"""
        cache_data = self._load_cache()
        return list(cache_data['tables'].keys())
    
    def get_records(self, table_name: str) -> List[Dict[str, Any]]:
        """Get all records from a table"""
        cache_data = self._load_cache()
        if table_name in cache_data['records']:
            return cache_data['records'][table_name]
        return []
    
    def get_record_by_id(self, table_name: str, record_id: int) -> Optional[Dict[str, Any]]:
        """Get a specific record by ID"""
        records = self.get_records(table_name)
        for record in records:
            if int(record['id']) == record_id:
                return record
        return None
    
    def get_table_schema(self, table_name: str) -> List[Dict[str, str]]:
        """Get schema information for a table"""
        cache_data = self._load_cache()
        if table_name in cache_data['tables']:
            return cache_data['tables'][table_name]['columns']
        return []
    
    def insert_record(self, table_name: str, record: Dict[str, Any]) -> bool:
        """Insert a new record into a table"""
        try:
            cache_data = self._load_cache()
            
            # Make sure the table exists
            if table_name not in cache_data['records']:
                cache_data['records'][table_name] = []
            
            # Check for duplicate ID
            for existing_record in cache_data['records'][table_name]:
                if int(existing_record['id']) == int(record['id']):
                    return False
            
            # Add the record
            cache_data['records'][table_name].append(record)
            self._save_cache(cache_data)
            
            # Generate the INSERT command
            db_name = cache_data['tables'][table_name]['database']
            values = []
            for column in cache_data['tables'][table_name]['columns']:
                col_name = column['name']
                if col_name in record:
                    if column['type'] == 'STRING':
                        values.append(f'"{record[col_name]}"')
                    else:
                        values.append(str(record[col_name]))
                else:
                    values.append('NULL')
            
            values_str = ", ".join(values)
            command = f"USE DATABASE {db_name}\n"
            command += f"USE TABLE {table_name}\n"
            command += f"INSERT INTO {table_name} VALUES ({values_str})"
            
            # Execute the command
            self.run_command(command)
            
            return True
        except Exception as e:
            print(f"Error inserting record: {e}")
            return False
    
    def update_record(self, table_name: str, record_id: int, record: Dict[str, Any]) -> bool:
        """Update an existing record"""
        try:
            cache_data = self._load_cache()
            
            # Make sure the table exists
            if table_name not in cache_data['records']:
                return False
            
            # Find and update the record
            for i, existing_record in enumerate(cache_data['records'][table_name]):
                if int(existing_record['id']) == int(record_id):
                    # Keep the same ID
                    record['id'] = existing_record['id']
                    cache_data['records'][table_name][i] = record
                    self._save_cache(cache_data)
                    
                    # Generate the UPDATE command for each field
                    db_name = cache_data['tables'][table_name]['database']
                    command = f"USE DATABASE {db_name}\n"
                    command += f"USE TABLE {table_name}\n"
                    
                    for key, value in record.items():
                        if key != 'id':
                            # Format the value based on type
                            if isinstance(value, str):
                                value_str = f'"{value}"'
                            else:
                                value_str = str(value)
                            
                            command += f"UPDATE {table_name} SET {key} = {value_str} WHERE id = {record_id}\n"
                    
                    # Execute the command
                    self.run_command(command)
                    
                    return True
            
            return False
        except Exception as e:
            print(f"Error updating record: {e}")
            return False
    
    def delete_record(self, table_name: str, record_id: int) -> bool:
        """Delete a record"""
        try:
            cache_data = self._load_cache()
            
            # Make sure the table exists
            if table_name not in cache_data['records']:
                return False
            
            # Find and remove the record
            records = cache_data['records'][table_name]
            initial_count = len(records)
            cache_data['records'][table_name] = [r for r in records if int(r['id']) != int(record_id)]
            
            if len(cache_data['records'][table_name]) < initial_count:
                self._save_cache(cache_data)
                
                # Generate the DELETE command
                db_name = cache_data['tables'][table_name]['database']
                command = f"USE DATABASE {db_name}\n"
                command += f"USE TABLE {table_name}\n"
                command += f"DELETE FROM {table_name} WHERE id = {record_id}"
                
                # Execute the command
                self.run_command(command)
                
                return True
            
            return False
        except Exception as e:
            print(f"Error deleting record: {e}")
            return False