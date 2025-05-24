from flask import Flask, render_template, request, redirect, url_for, flash, jsonify
import subprocess
import json
import os
from config import Config
from db_connector import DatabaseConnector

app = Flask(__name__)
app.config.from_object(Config)
db = DatabaseConnector(app.config['DATABASE_PATH'])

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
        # Collect form data
        user_data = {
            'id': int(request.form['id']),
            'name': request.form['name'],
            'email': request.form['email']
        }
        
        # Validate data
        errors = []
        if not user_data['name']:
            errors.append('Name is required')
        if not user_data['email']:
            errors.append('Email is required')
        
        # Check if ID already exists
        if db.get_record_by_id('users', user_data['id']):
            errors.append(f"User with ID {user_data['id']} already exists")
            
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
            'id': user_id,  # Keep the same ID
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
    user = db.get_record_by_id('users', user_id)
    if not user:
        flash('User not found', 'error')
        return redirect(url_for('list_users'))
    
    if db.delete_record('users', user_id):
        flash('User deleted successfully!', 'success')
    else:
        flash('Failed to delete user', 'error')
    
    return redirect(url_for('list_users'))

# API endpoints
@app.route('/api/users', methods=['GET'])
def api_users():
    """API to get all users"""
    users = db.get_records('users')
    return jsonify(users)

@app.route('/api/users/<int:user_id>', methods=['GET'])
def api_user(user_id):
    """API to get a specific user"""
    user = db.get_record_by_id('users', user_id)
    if not user:
        return jsonify({'error': 'User not found'}), 404
    return jsonify(user)

@app.errorhandler(404)
def page_not_found(e):
    """Handle 404 errors"""
    return render_template('error.html', error="Page not found"), 404

@app.errorhandler(500)
def internal_server_error(e):
    """Handle 500 errors"""
    return render_template('error.html', error="Internal server error"), 500

if __name__ == '__main__':
    app.run(debug=True)