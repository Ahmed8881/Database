import os

class Config:
    """Configuration settings"""
    # Secret key for session management
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-key-change-in-production'
    
    # Path to the database directory
    DATABASE_PATH = os.environ.get('DATABASE_PATH') or os.path.join(os.path.dirname(os.path.abspath(__file__)), '../Database')
    
    # Database binary location
    DB_BINARY = os.environ.get('DB_BINARY') or os.path.join(DATABASE_PATH, 'bin/db-project')