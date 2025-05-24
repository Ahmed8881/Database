#!/usr/bin/env python
"""
Test raw socket communication with the database server.
"""
import socket
import json
import time
import sys
import argparse

def test_raw_socket(host="localhost", port=9000, command_type="select"):
    """Test raw socket communication with the server"""
    print(f"Opening direct socket connection to {host}:{port}...")
    
    # Select command based on type
    if command_type.lower() == "select":
        command = {
            "command": "select",
            "table": "users",
            "columns": ["*"]
        }
    elif command_type.lower() == "insert":
        command = {
            "command": "insert",
            "table": "users",
            "values": [3, "Bob", "bob@example.com"]
        }
    elif command_type.lower() == "begin":
        command = {
            "command": "begin"
        }
    else:
        command = {
            "command": command_type
        }
    
    s = None
    try:
        # Create a simple socket directly
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        print("Connected!")
        
        # Encode command
        print(f"Sending command: {json.dumps(command)}")
        cmd_bytes = json.dumps(command).encode('utf-8')
        length = len(cmd_bytes).to_bytes(4, byteorder='big')
        
        # Send command
        s.sendall(length + cmd_bytes)
        print("Command sent!")
        
        # Try to read response
        try:
            print("Waiting for response...")
            # Read length prefix (4 bytes)
            length_bytes = s.recv(4)
            if length_bytes:
                length = int.from_bytes(length_bytes, byteorder='big')
                print(f"Receiving response of {length} bytes")
                
                # Read response data
                response_bytes = b''
                while len(response_bytes) < length:
                    chunk = s.recv(length - len(response_bytes))
                    if not chunk:
                        raise Exception("Connection closed during response read")
                    response_bytes += chunk
                
                # Parse response
                response_str = response_bytes.decode('utf-8')
                response = json.loads(response_str)
                print("\nReceived response:")
                print(json.dumps(response, indent=2))
                
        except Exception as e:
            print(f"Error reading response: {e}")
        
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        try:
            if s is not None:
                s.close()
                print("Connection closed")
        except:
            pass

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Test raw socket communication")
    parser.add_argument("--host", default="localhost", help="Server hostname")
    parser.add_argument("--port", type=int, default=9000, help="Server port")
    parser.add_argument("--command", default="select", 
                      help="Command type (select, insert, begin, etc.)")
    
    args = parser.parse_args()
    return test_raw_socket(args.host, args.port, args.command)

if __name__ == "__main__":
    sys.exit(main())