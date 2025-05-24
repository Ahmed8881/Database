import socket
import json
import struct
import threading
import time

def handle_client(client_socket, address):
    """Handle a client connection"""
    print(f"Client connected from {address[0]}:{address[1]}")
    
    try:
        while True:
            # Read message length (4 bytes)
            length_bytes = client_socket.recv(4)
            if not length_bytes:
                print("Client disconnected")
                break
                
            # Parse message length
            message_length = int.from_bytes(length_bytes, byteorder='big')
            print(f"Receiving message of {message_length} bytes")
            
            # Read message data
            message_data = b''
            while len(message_data) < message_length:
                chunk = client_socket.recv(message_length - len(message_data))
                if not chunk:
                    raise Exception("Connection closed during message read")
                message_data += chunk
            
            # Decode message
            message_json = message_data.decode('utf-8')
            command = json.loads(message_json)
            
            print("\nReceived command:")
            print("-" * 50)
            print(json.dumps(command, indent=2))
            print("-" * 50)
            
            # Create response based on command
            from typing import Any
            response: dict[str, Any] = {"success": True}
            
            # Handle different command types
            cmd_type = command.get("command", "").lower()
            
            if cmd_type == "begin":
                response["transaction_id"] = 12345
                response["message"] = "Transaction started"
                
            elif cmd_type == "commit":
                response["message"] = "Transaction committed"
                
            elif cmd_type == "rollback":
                response["message"] = "Transaction rolled back"
                
            elif cmd_type == "select":
                response["message"] = "Query executed"
                response["rows"] = [
                    {"id": 1, "name": "John Doe", "email": "john@example.com"},
                    {"id": 2, "name": "Jane Smith", "email": "jane@example.com"}
                ]
                
            elif cmd_type == "insert":
                response["message"] = "1 row(s) inserted"
                response["row_count"] = 1
                
            elif cmd_type == "update":
                response["message"] = "1 row(s) updated"
                response["row_count"] = 1
                
            elif cmd_type == "delete":
                response["message"] = "1 row(s) deleted"
                response["row_count"] = 1
                
            elif cmd_type == "create_table":
                response["message"] = "Table created"
                
            elif cmd_type == "create_index":
                response["message"] = "Index created"
                
            elif cmd_type == "create_database":
                response["message"] = "Database created"
                
            elif cmd_type == "use_database":
                response["message"] = f"Using database '{command.get('database')}'"
                
            elif cmd_type == "use_table":
                response["message"] = f"Using table '{command.get('table')}'"
                
            elif cmd_type == "show_tables":
                response["message"] = "Tables retrieved"
                response["tables"] = ["users", "products", "orders"]
                
            elif cmd_type == "show_indexes":
                response["message"] = "Indexes retrieved"
                response["indexes"] = [
                    {"name": "idx_user_name", "table": "users", "column": "name"},
                    {"name": "idx_user_email", "table": "users", "column": "email"}
                ]
            else:
                response["success"] = False
                response["message"] = f"Unknown command: {cmd_type}"
            
            # Add transaction ID if it was in the request
            if "transaction_id" in command:
                response["transaction_id"] = command["transaction_id"]
            
            # Send response
            response_json = json.dumps(response)
            response_bytes = response_json.encode('utf-8')
            length_bytes = len(response_bytes).to_bytes(4, byteorder='big')
            
            client_socket.sendall(length_bytes + response_bytes)
            print(f"Sent response: {response_json}")
    
    except Exception as e:
        print(f"Error handling client: {e}")
    
    finally:
        client_socket.close()
        print(f"Connection closed with {address[0]}:{address[1]}")

def start_server(host='0.0.0.0', port=9000):
    """Start a mock database server"""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"Mock database server listening on {host}:{port}")
        
        while True:
            client_socket, address = server_socket.accept()
            client_thread = threading.Thread(
                target=handle_client, 
                args=(client_socket, address)
            )
            client_thread.daemon = True
            
            client_thread.daemon = True
            client_thread.start()
            
    except KeyboardInterrupt:
        print("\nShutting down server...")
    finally:
        server_socket.close()

if __name__ == "__main__":
    start_server()