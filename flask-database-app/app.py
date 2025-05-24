from flask import Flask, render_template, request, redirect, url_for, flash, jsonify
import subprocess
import json
import os
from db_connector import DatabaseConnector

app = Flask(__name__)
app.config['SECRET_KEY'] = 'your_secret_key_here'

# Get the absolute path to the root of your project
project_root = os.path.dirname(os.path.abspath(__file__))
db_root = os.path.dirname(project_root)  # Parent directory of flask-database-app

# Initialize database connector
db = DatabaseConnector(db_root)

@app.route('/')
def index():
    """Main dashboard showing database statistics"""
    tables = db.get_tables()
    table_stats = {}
    for table in tables:
        count = len(db.get_records(table))
        table_stats[table] = count
    
    return render_template('index.html', tables=tables, table_stats=table_stats)

# User routes
@app.route('/users')
def list_users():
    """List all users"""
    users = db.get_records('users')
    return render_template('users/list.html', users=users)

@app.route('/users/<int:user_id>')
def view_user(user_id):
    """View user details"""
    user = db.get_record_by_id('users', user_id)
    if not user:
        flash('User not found', 'error')
        return redirect(url_for('list_users'))
    
    return render_template('users/view.html', user=user)

@app.route('/users/create', methods=['GET', 'POST'])
def create_user():
    """Create a new user"""
    if request.method == 'POST':
        # Get the next available ID
        next_id = db.get_next_id('users')
        
        # Collect form data
        user_data = {
            'id': next_id,
            'name': request.form['name'],
            'email': request.form['email']
        }
        
        # Validate data
        errors = []
        if not user_data['name']:
            errors.append('Name is required')
        if not user_data['email']:
            errors.append('Email is required')
            
        if errors:
            for error in errors:
                flash(error, 'error')
            return render_template('users/create.html', user=user_data)
        
        # Insert user
        if db.insert_record('users', user_data):
            flash('User created successfully!', 'success')
            return redirect(url_for('list_users'))
        else:
            flash('Failed to create user', 'error')
            return render_template('users/create.html', user=user_data)
    
    return render_template('users/create.html', user={})

@app.route('/users/<int:user_id>/edit', methods=['GET', 'POST'])
def edit_user(user_id):
    """Edit an existing user"""
    user = db.get_record_by_id('users', user_id)
    if not user:
        flash('User not found', 'error')
        return redirect(url_for('list_users'))
    
    if request.method == 'POST':
        # Collect form data
        user_data = {
            'id': user_id,
            'name': request.form['name'],
            'email': request.form['email']
        }
        
        # Validate data
        errors = []
        if not user_data['name']:
            errors.append('Name is required')
        if not user_data['email']:
            errors.append('Email is required')
            
        if errors:
            for error in errors:
                flash(error, 'error')
            return render_template('users/edit.html', user=user_data)
        
        # Update user
        if db.update_record('users', user_id, user_data):
            flash('User updated successfully!', 'success')
            return redirect(url_for('view_user', user_id=user_id))
        else:
            flash('Failed to update user', 'error')
            return render_template('users/edit.html', user=user_data)
    
    return render_template('users/edit.html', user=user)

@app.route('/users/<int:user_id>/delete', methods=['POST'])
def delete_user(user_id):
    """Delete a user"""
    if db.delete_record('users', user_id):
        flash('User deleted successfully!', 'success')
    else:
        flash('Failed to delete user', 'error')
    
    return redirect(url_for('list_users'))

if __name__ == '__main__':
    app.run(debug=True)