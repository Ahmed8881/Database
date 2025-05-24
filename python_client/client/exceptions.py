"""
Custom exceptions for the database client
"""

class DatabaseError(Exception):
    """Base class for all database-related exceptions"""
    pass

class ConnectionError(DatabaseError):
    """Exception raised for database connection errors"""
    pass

class QueryError(DatabaseError):
    """Exception raised for errors in SQL queries"""
    pass

class ParseError(DatabaseError):
    """Exception raised for SQL parsing errors"""
    pass

class TransactionError(DatabaseError):
    """Exception raised for transaction-related errors"""
    pass