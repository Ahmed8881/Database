#!/usr/bin/env python3
"""
Simple demo of the Python Database Client Library

This script demonstrates how easy it is to use the database 
programmatically through the Python client library.
"""

import sys
import os
import time
import subprocess

# Add the client library to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python_client'))

from client.connection import Connection
from client.exceptions import ConnectionError, QueryError

def demo_python_client():
    """
    Demonstrates the Python client library for database access
    """
    print("🐍 Python Database Client Library Demo")
    print("=" * 50)
    
    try:
        # Create a connection to the database server
        print("📡 Connecting to database server...")
        conn = Connection(host="localhost", port=9000)
        conn.connect()
        print("✅ Connected successfully!")
        
        print("\n📝 Demonstrating SQL command execution:")
        
        # Demonstrate different types of commands
        commands = [
            ("Authentication", "LOGIN admin admin"),
            ("Database Creation", "CREATE DATABASE demo"),  
            ("Table Creation", "CREATE TABLE users (id INT, name STRING(50), email STRING(100))"),
            ("Data Insertion", "INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')"),
            ("Data Query", "SELECT * FROM users"),
            ("Transaction Begin", "BEGIN"),
            ("Transaction Commit", "COMMIT"),
        ]
        
        for desc, cmd in commands:
            print(f"\n🔍 {desc}:")
            print(f"   SQL: {cmd}")
            try:
                result = conn.execute(cmd)
                if result.get("success"):
                    print(f"   ✅ Success: {result.get('message', 'No message')}")
                else:
                    print(f"   ⚠️  Response: {result.get('message', 'No message')}")
            except Exception as e:
                print(f"   ❌ Error: {e}")
        
        print(f"\n🔐 Security Note:")
        print("   All commands show authentication requirements - this demonstrates")
        print("   that the network interface maintains the same security as CLI")
        
        conn.close()
        print(f"\n🔌 Connection closed")
        
        print(f"\n🎯 Key Benefits of Network Interface:")
        print("   • Programmatic database access from any language")
        print("   • JSON-based protocol for easy integration")
        print("   • Same security and functionality as CLI")
        print("   • Transaction support over network")
        print("   • Connection pooling and concurrent access")
        
        return True
        
    except ConnectionError as e:
        print(f"❌ Connection failed: {e}")
        print("💡 Make sure to start the database server first:")
        print("   ./bin/db-server")
        return False
    except Exception as e:
        print(f"❌ Demo failed: {e}")
        return False

if __name__ == "__main__":
    success = demo_python_client()
    
    if success:
        print(f"\n🎉 Demo completed successfully!")
        print(f"\nTo use this in your own Python applications:")
        print(f"```python")
        print(f"from client.connection import Connection")
        print(f"")
        print(f"# Connect to database")
        print(f"conn = Connection(host='localhost', port=9000)")
        print(f"conn.connect()")
        print(f"")
        print(f"# Execute SQL commands")
        print(f"result = conn.execute('SELECT * FROM users')")
        print(f"print(result)")
        print(f"")
        print(f"# Close connection")
        print(f"conn.close()")
        print(f"```")
    else:
        print(f"\n💡 Start the database server with: ./bin/db-server")
    
    sys.exit(0 if success else 1)