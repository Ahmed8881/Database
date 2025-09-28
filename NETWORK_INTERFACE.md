# Network Interface Documentation

The JHAZ Database System now supports both command-line and network interfaces, enabling programmatic access through TCP connections with JSON protocol.

## Quick Start

### Starting the Database Server

```bash
# Build the project
make

# Start the network server (listens on port 9000)
./bin/db-server

# Or run the CLI version
./bin/db-cli
```

### Using the Python Client Library

```python
from client.connection import Connection

# Connect to database server
conn = Connection(host='localhost', port=9000)
conn.connect()

# Execute SQL commands
result = conn.execute('LOGIN admin admin')
result = conn.execute('CREATE DATABASE myapp')  
result = conn.execute('CREATE TABLE users (id INT, name STRING(50), email STRING(100))')
result = conn.execute('INSERT INTO users VALUES (1, "Alice", "alice@example.com")')
result = conn.execute('SELECT * FROM users')

# Transaction support
conn.begin()
conn.execute('INSERT INTO users VALUES (2, "Bob", "bob@example.com")')
conn.commit()

# Close connection
conn.close()
```

## Architecture

### Dual Interface Design

The system provides two complementary interfaces:

1. **CLI Mode** (`./bin/db-cli`): Interactive command-line interface for administration
2. **Network Mode** (`./bin/db-server`): TCP server with JSON API for programmatic access

### Network Protocol

- **Transport**: TCP on port 9000 (configurable)
- **Framing**: 4-byte length prefix (network byte order) + JSON payload
- **Format**: JSON request/response messages
- **Security**: Same authentication system as CLI

### Message Format

**Request:**
```json
{
  "command": "select",
  "table": "users",
  "columns": ["*"],
  "where": {
    "column": "id",
    "operator": "=", 
    "value": 1
  }
}
```

**Response:**
```json
{
  "success": true,
  "message": "Query executed successfully",
  "rows": [
    {"id": 1, "name": "Alice", "email": "alice@example.com"}
  ]
}
```

## Supported Commands

The Python client parser supports all major SQL operations:

### Database Operations
- `CREATE DATABASE name`
- `USE DATABASE name`

### Table Operations  
- `CREATE TABLE name (columns...)`
- `USE TABLE name`
- `SHOW TABLES`

### Data Operations
- `SELECT [columns] FROM table [WHERE condition]`
- `INSERT INTO table VALUES (values...)`
- `UPDATE table SET column=value WHERE condition`
- `DELETE FROM table WHERE condition`

### Transaction Operations
- `BEGIN` - Start transaction
- `COMMIT` - Commit transaction  
- `ROLLBACK` - Rollback transaction

### Authentication
- `LOGIN username password`

### Index Operations
- `CREATE INDEX name ON table (column)`
- `SHOW INDEXES FROM table`

## Python Client Library

### Connection Class

```python
from client.connection import Connection

conn = Connection(
    host="localhost",      # Server host
    port=9000,             # Server port  
    database="mydb",       # Optional: auto-use database
    connect_timeout=30     # Connection timeout
)
```

### Methods

- `connect()` - Establish connection
- `execute(sql)` - Execute SQL command, returns dict
- `begin()` - Start transaction
- `commit()` - Commit transaction
- `rollback()` - Rollback transaction
- `close()` - Close connection

### Context Manager Support

```python
with Connection(host='localhost', port=9000) as conn:
    result = conn.execute('SELECT * FROM users')
    print(result)
# Connection automatically closed
```

### Error Handling

```python
from client.exceptions import ConnectionError, QueryError

try:
    conn = Connection('localhost', 9000)
    conn.connect()
    result = conn.execute('SELECT * FROM users')
except ConnectionError as e:
    print(f"Connection failed: {e}")
except QueryError as e:
    print(f"Query failed: {e}")
```

## Security

The network interface maintains the same security model as the CLI:

- **Authentication Required**: Most operations require login
- **Session Management**: Each connection maintains its own session state
- **Secure by Default**: No default credentials or open access

## Testing

### Running the Test Suite

```bash
# Test the network interface
python3 /tmp/test_network_interface.py

# Run the demo
PYTHONPATH=python_client python3 demo_python_client.py
```

### Manual Testing

```bash
# Terminal 1: Start server
./bin/db-server

# Terminal 2: Test with Python
cd python_client
python3 test_cases/test_connection.py --sequence
```

## Performance & Scalability

### Features
- **Thread Pool**: Concurrent connection handling
- **Connection Pooling**: Efficient resource management  
- **Non-blocking I/O**: Responsive server performance
- **Transaction Management**: ACID compliance over network

### Configuration
- Default port: 9000
- Max connections: 100 (configurable)
- Connection timeout: 60 seconds
- Buffer size: 4KB per connection

## Integration Examples

### Web Application (Flask)
```python
from flask import Flask, request, jsonify
from client.connection import Connection

app = Flask(__name__)

@app.route('/users', methods=['GET'])
def get_users():
    with Connection('localhost', 9000) as conn:
        result = conn.execute('SELECT * FROM users')
        return jsonify(result)

@app.route('/users', methods=['POST'])
def create_user():
    data = request.json
    with Connection('localhost', 9000) as conn:
        result = conn.execute(f"INSERT INTO users VALUES ({data['id']}, '{data['name']}', '{data['email']}')")
        return jsonify(result)
```

### Microservice Integration
```python
import asyncio
from client.connection import Connection

class DatabaseService:
    def __init__(self):
        self.conn = Connection('database-server', 9000)
    
    async def get_user(self, user_id):
        result = self.conn.execute(f'SELECT * FROM users WHERE id = {user_id}')
        return result.get('rows', [])
    
    async def create_user(self, user_data):
        sql = f"INSERT INTO users VALUES ({user_data['id']}, '{user_data['name']}', '{user_data['email']}')"
        return self.conn.execute(sql)
```

## Migration Guide

### From CLI to Network
1. Start the database server: `./bin/db-server`
2. Replace command-line scripts with Python client calls
3. Update authentication to use `LOGIN` commands
4. Wrap operations in try/catch for error handling

### Backwards Compatibility
- CLI interface remains fully functional (`./bin/db-cli`)
- Same SQL commands and syntax
- Identical authentication and security model
- Same database files and data formats

## Troubleshooting

### Common Issues

**Connection Refused:**
```
ConnectionError: Failed to connect to database at localhost:9000
```
- Ensure server is running: `./bin/db-server`
- Check port availability: `netstat -an | grep 9000`

**JSON Parse Errors:**
```
QueryError: Invalid JSON
```
- Check command syntax in Python client
- Ensure proper escaping of strings with quotes

**Authentication Errors:**
```
{"success": true, "message": "Authentication required"}
```
- Login first: `conn.execute('LOGIN username password')`
- Create user if needed through CLI first

### Debug Mode

Start server with debug output:
```bash
# Enable debug logging
DEBUG=1 ./bin/db-server
```

## Conclusion

The network interface successfully transforms the JHAZ Database System from a CLI-only tool into a full network-accessible database server, enabling:

- **Programmatic Access**: Use from any language via TCP/JSON
- **Web Integration**: Easy integration with web applications
- **Microservice Architecture**: Database as a service component
- **Concurrent Access**: Multiple clients simultaneously
- **Maintained Security**: Same authentication as CLI

The Python client library provides a clean, Pythonic interface while the server maintains full compatibility with existing CLI functionality.