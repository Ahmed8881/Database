#!/usr/bin/env python
"""
Test the database connection.
"""
import sys
import os
import json
import argparse

# Add the parent directory to the Python path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

# Now import your modules
from client.connection import Connection
from client.exceptions import ConnectionError, QueryError

def execute_query(host, port, database, query):
    """Execute a single query against the database"""
    try:
        conn = Connection(host=host, port=port, database=database)
        conn.connect()
        
        print(f"Connected to {host}:{port}")
        print(f"Executing: {query}")
        
        result = conn.execute(query)
        print(json.dumps(result, indent=2))
        
        conn.close()
        return 0
    except ConnectionError as e:
        print(f"Connection error: {e}")
        return 1
    except QueryError as e:
        print(f"Query error: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        return 1

def run_test_sequence():
    """Run a sequence of test queries"""
    try:
        # Create connection
        conn = Connection(host="localhost", port=9000)
        print("Connecting to database...")
        conn.connect()
        print("Connected successfully!")
        
        # Try some queries
        print("\n1. Testing SELECT")
        result = conn.execute("SELECT * FROM users")
        print(json.dumps(result, indent=2))
        
        print("\n2. Testing transaction")
        conn.begin()
        result = conn.execute("INSERT INTO users VALUES (3, 'Alice', 'alice@example.com')")
        print(json.dumps(result, indent=2))
        conn.commit()
        
        print("\n3. Testing CREATE TABLE")
        result = conn.execute("CREATE TABLE products (id INT, name STRING, price FLOAT)")
        print(json.dumps(result, indent=2))
        
        # Close connection
        conn.close()
        print("\nConnection closed")
        return 0
        
    except ConnectionError as e:
        print(f"Connection error: {e}")
        return 1
    except QueryError as e:
        print(f"Query error: {e}")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        return 1

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Test database connection')
    parser.add_argument('--host', default='localhost', help='Database host')
    parser.add_argument('--port', type=int, default=9000, help='Database port')
    parser.add_argument('--database', help='Database name')
    parser.add_argument('query', nargs='?', help='SQL query to execute')
    parser.add_argument('--sequence', action='store_true', help='Run test sequence')
    
    args = parser.parse_args()
    
    if args.sequence:
        return run_test_sequence()
    elif args.query:
        return execute_query(args.host, args.port, args.database, args.query)
    else:
        print("Error: No action specified. Use --sequence or provide a query.")
        parser.print_help()
        return 1

if __name__ == '__main__':
    sys.exit(main())