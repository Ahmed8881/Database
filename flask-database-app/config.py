import os

class Config:
    """Configuration settings"""
    # Secret key for session management
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-key-change-in-production'
    
    # Path to the database directory
    DATABASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    # Database binary location
    DB_BINARY = os.path.join(DATABASE_PATH, 'bin', 'db-project')
    
    # Database and table names
    DATABASE_NAME = 'myusers'
    USERS_TABLE = 'users'