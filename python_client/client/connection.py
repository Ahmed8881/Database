import re
import json
import socket
from typing import Dict, List, Any, Optional

from .parser import SQLParser
from .exceptions import ConnectionError, QueryError

class Connection:
    """
    Connection to the database server via socket communication.
    """
    def __init__(self, 
                 host: str = "localhost", 
                 port: int = 9000,
                 database: Optional[str] = None,
                 connect_timeout: int = 30):
        
        self.host = host
        self.port = port
        self.database = database
        self.connect_timeout = connect_timeout
        
        # Connection state
        self.socket = None
        self.connected = False
        self.transaction_id = None
        self.parser = SQLParser()
    
    def connect(self):
        """Connect to the database server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(self.connect_timeout)
            self.socket.connect((self.host, self.port))
            self.connected = True
            
            # If database is specified, send USE DATABASE command
            if self.database:
                self.execute(f"USE DATABASE {self.database}")
                
            return True
                
        except socket.error as e:
            self.connected = False
            self.socket = None
            raise ConnectionError(f"Failed to connect to database at {self.host}:{self.port}: {e}")
    
    def _send_command(self, command_obj: Dict[str, Any]) -> Dict[str, Any]:
        """Send a command to the database server and get response"""
        if not self.connected or not self.socket:
            raise ConnectionError("Not connected to database")
        
        # Add transaction ID if available
        if self.transaction_id:
            command_obj["transaction_id"] = self.transaction_id
            
        # Convert to JSON and send
        command_json = json.dumps(command_obj)
        command_bytes = command_json.encode('utf-8')
        
        # Prefix with length for message framing
        length_prefix = len(command_bytes).to_bytes(4, byteorder='big')
        
        try:
            self.socket.sendall(length_prefix + command_bytes)
            
            # Read response length (4 bytes)
            length_bytes = self._recv_all(4)
            length = int.from_bytes(length_bytes, byteorder='big')
            
            # Read response data
            response_bytes = self._recv_all(length)
            response_json = response_bytes.decode('utf-8')
            response = json.loads(response_json)
            
            # Check for transaction ID in response
            if "transaction_id" in response:
                self.transaction_id = response["transaction_id"]
                
            # Check for errors
            if "error" in response:
                raise QueryError(response["error"])
                
            return response
            
        except socket.error as e:
            self.connected = False
            self.socket = None
            raise ConnectionError(f"Connection lost: {e}")
    
    def _recv_all(self, n: int) -> bytes:
        """Receive exactly n bytes"""
        data = b''
        while len(data) < n:
            packet = self.socket.recv(n - len(data))
            if not packet:
                raise ConnectionError("Connection closed by server")
            data += packet
        return data
    
    def execute(self, sql: str) -> Dict[str, Any]:
        """Execute SQL command and return response"""
        # Parse SQL into JSON command
        command_obj = self.parser.parse(sql)
        
        # Send command and get response
        response = self._send_command(command_obj)
        return response
    
    def begin(self):
        """Begin a transaction"""
        return self.execute("BEGIN")
    
    def commit(self):
        """Commit the current transaction"""
        result = self.execute("COMMIT")
        self.transaction_id = None
        return result
    
    def rollback(self):
        """Rollback the current transaction"""
        result = self.execute("ROLLBACK")
        self.transaction_id = None
        return result
    
    def close(self):
        """Close the connection"""
        if self.connected and self.socket:
            try:
                if self.transaction_id:
                    self.rollback()
                self.socket.close()
            except:
                pass
            finally:
                self.socket = None
                self.connected = False
    
    def __enter__(self):
        if not self.connected:
            self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()