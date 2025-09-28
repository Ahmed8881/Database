# Database Project

## Overview

The **Database Project** is a lightweight, command-line-based database engine implemented in **C**. Inspired by SQLite, it offers fundamental functionalities for managing and querying data using a B-Tree data structure. This project serves as an educational tool to understand the core principles behind database systems, including data storage, indexing, and query processing.

---

## Features

- **Dual Interface Support**: 
  - **CLI Mode**: Interactive command-line interface (`./bin/db-cli`)
  - **Network Mode**: TCP server with JSON API (`./bin/db-server`)
- **Python Client Library**: Complete Python client for programmatic access
- **Multi-Database Support:** Create and manage multiple databases
- **Table Management:** Create tables with various data types
- **Full CRUD Operations:** Insert, Select, Update, Delete records
- **Advanced Querying:** Filter by any column, select specific columns
- **B-Tree Indexing:** Efficient data organization and retrieval using B-Trees
- **Transaction Support:** ACID transactions with BEGIN/COMMIT/ROLLBACK
- **Authentication System:** Secure user management and access control
- **Network Protocol:** JSON-based TCP communication for remote access
- **Concurrent Access:** Multi-threaded server supporting multiple clients
- **Meta-Commands:** Special commands prefixed with `.` for debugging and administration

---

## Getting Started

### Prerequisites

- C compiler (GCC recommended)
- Make
- Unix-like environment (Linux, macOS, or WSL on Windows)

### Installation

1. **Clone the Repository:**

   ```sh
   git clone https://github.com/Ahmed8881/Database.git
   cd Database
   ```

2. **Build the Project:**

   Use the provided `Makefile` to compile the source code.

   ```sh
   make
   ```

   - This command compiles the C source files and generates both executables:
     - `bin/db-cli` - Interactive command-line interface
     - `bin/db-server` - Network server with JSON API

   For a debug build with additional debug symbols and flags:

   ```sh
   make DEBUG=1
   ```

   - This compiles the code with debug flags (`-DDEBUG`, `-O0`) for easier troubleshooting.
3. **Clean Build Artifacts (Optional):**

   To remove compiled objects and binaries, run:

   ```sh
   make clean
   ```

4. **Run the Application:**

   **CLI Mode (Interactive):**
   ```sh
   ./bin/db-cli
   ```

   **Network Server Mode:**
   ```sh
   ./bin/db-server
   ```
   The server will start on port 9000 and accept JSON-based TCP connections.

   **Python Client Usage:**
   ```python
   # Add to your Python script
   from python_client.client.connection import Connection
   
   conn = Connection(host='localhost', port=9000)
   conn.connect()
   result = conn.execute('SELECT * FROM users')
   conn.close()
   ```

---

## Network Interface

The database now supports network access through a TCP server with JSON protocol, enabling programmatic access from any language.

### Starting the Network Server

```sh
# Start the database server (listens on port 9000)
./bin/db-server
```

### Python Client Library

A complete Python client library is included in the `python_client/` directory:

```python
from client.connection import Connection

# Connect to database server
with Connection(host='localhost', port=9000) as conn:
    # Authenticate
    conn.execute('LOGIN admin admin')
    
    # Create and use database
    conn.execute('CREATE DATABASE myapp')
    conn.execute('USE DATABASE myapp')
    
    # Create table and insert data
    conn.execute('CREATE TABLE users (id INT, name STRING(50), email STRING(100))')
    conn.execute('INSERT INTO users VALUES (1, "Alice", "alice@example.com")')
    
    # Query data
    result = conn.execute('SELECT * FROM users')
    print(result)
    
    # Transaction support
    conn.begin()
    conn.execute('INSERT INTO users VALUES (2, "Bob", "bob@example.com")')
    conn.commit()
```

### Protocol Details

- **Transport**: TCP on port 9000
- **Format**: JSON messages with 4-byte length prefix
- **Authentication**: Same security model as CLI
- **Features**: Full SQL support, transactions, concurrent connections

See [`NETWORK_INTERFACE.md`](NETWORK_INTERFACE.md) for complete documentation.

---

## Usage

Upon running the application, you'll enter an interactive shell where you can execute SQL-like commands and meta-commands.

### Database Management Commands

- **Create a Database:**

  ```sql
  CREATE DATABASE database_name
  ```

  Example:
  ```sql
  CREATE DATABASE school
  ```

- **Use a Database:**

  ```sql
  USE DATABASE database_name
  ```

  Example:
  ```sql
  USE DATABASE school
  ```

### Table Management Commands

- **Create a Table:**

  ```sql
  CREATE TABLE table_name (column1 type1, column2 type2, ...)
  ```

  Supported column types:
  - `INT` - Integer values
  - `STRING(n)` - Text of length n (default 255)
  - `FLOAT` - Floating point values
  - `BOOLEAN` - True/False values
  - `DATE` - Date values
  - `TIME` - Time values
  - `TIMESTAMP` - Combined date and time values
  - `BLOB(n)` - Binary data of size n (default 1024)

  Example:
  ```sql
  CREATE TABLE students (id INT, name STRING(50), gpa FLOAT)
  ```

- **Use a Table:**

  ```sql
  USE TABLE table_name
  ```

  Example:
  ```sql
  USE TABLE students
  ```

- **Show Tables:**

  ```sql
  SHOW TABLES
  ```

### Data Manipulation Commands

- **Insert Data:**

  ```sql
  INSERT INTO table_name VALUES (value1, value2, ...)
  ```

  Example:
  ```sql
  INSERT INTO students VALUES (1, "Alice", 3.8)
  ```

  For string values, you can use either single or double quotes.

- **Select All Data:**

  ```sql
  SELECT * FROM table_name
  ```

  Example:
  ```sql
  SELECT * FROM students
  ```

- **Select Specific Columns:**

  ```sql
  SELECT column1, column2, ... FROM table_name
  ```

  Example:
  ```sql
  SELECT name, gpa FROM students
  ```

- **Filter Records by Any Column:**

  ```sql
  SELECT * FROM table_name WHERE column_name = value
  ```

  Example:
  ```sql
  SELECT * FROM students WHERE name = "Bob"
  SELECT * FROM students WHERE gpa = 3.5
  ```

- **Combine Column Selection with Filtering:**

  ```sql
  SELECT column1, column2, ... FROM table_name WHERE column_name = value
  ```

  Example:
  ```sql
  SELECT name FROM students WHERE gpa = 3.5
  ```

- **Update Data:**

  ```sql
  UPDATE table_name SET column = value WHERE id = <id>
  ```

  Example:
  ```sql
  UPDATE students SET name = "Alicia" WHERE id = 1
  ```

  ```sql
  UPDATE students SET gpa = 4.0 WHERE id = 1
  ```

- **Delete Data:**

  ```sql
  DELETE FROM table_name WHERE id = <id>
  ```

  Example:
  ```sql
  DELETE FROM students WHERE id = 1
  ```

### Transaction Commands

Transactions ensure that database operations are atomic, consistent, isolated, and durable (ACID). 

- **Enable Transactions:**

- **Begin a Transaction:**

- **Commit a Transaction:**

- **Rollback a Transaction:**

- **View Transaction Status:**

- **Disable Transactions:**

### Meta-Commands

- **Exit the Application:**

  ```
  .exit
  ```

- **View B-Tree Structure:**

  ```
  .btree
  ```

- **View Constants:**

  ```
  .constants
  ```

### Example Session

```sh
db > CREATE DATABASE school
Executed.
db > USE DATABASE school
Executed.
db > CREATE TABLE students (id INT, name STRING(50), gpa FLOAT)
Executed.
db > USE TABLE students
Executed.
db > INSERT INTO students VALUES (1, "Alice", 3.8)
Executed.
db > INSERT INTO students VALUES (2, "Bob", 3.5)
Executed.
db > INSERT INTO students VALUES (3, "Carol", 3.5)
Executed.
db > SELECT * FROM students
| id | name | gpa |
|----------|----------|----------|
| 1 | Alice | 3.80 |
| 2 | Bob | 3.50 |
| 3 | Carol | 3.50 |
Executed.
db > SELECT name, gpa FROM students
| name | gpa |
|----------|----------|
| Alice | 3.80 |
| Bob | 3.50 |
| Carol | 3.50 |
Executed.
db > SELECT * FROM students WHERE gpa = 3.5
| id | name | gpa |
|----------|----------|----------|
| 2 | Bob | 3.50 |
| 3 | Carol | 3.50 |
Executed.
db > SELECT name FROM students WHERE gpa = 3.5
| name |
|----------|
| Bob |
| Carol |
Executed.
db > SELECT * FROM students WHERE id = 1
| id | name | gpa |
|----------|----------|----------|
| 1 | Alice | 3.80 |
Executed.
db > UPDATE students SET name = "Alicia" WHERE id = 1
Executed.
db > SELECT * FROM students WHERE id = 1
| id | name | gpa |
|----------|----------|----------|
| 1 | Alicia | 3.80 |
Executed.
db > DELETE FROM students WHERE id = 1
Executed.
db > SELECT * FROM students
| id | name | gpa |
|----------|----------|----------|
| 2 | Bob | 3.50 |
| 3 | Carol | 3.50 |
Executed.
db > .exit
```

---

## Project Structure

The project files are organized as follows:

```
Database/
├── Database/
|   ├── Database_name            # Database storage directory
|       ├── Tables               # Tables storage directory
|           ├── .tbl files
|       ├── .catalog files       # for storing information about tables
├── include/                     # Header files for the C source code
│   ├── btree.h
│   ├── command_processor.h
│   ├── cursor.h
│   ├── input_handling.h
│   ├── pager.h
│   ├── queue.h
│   ├── stack.h
│   ├── table.h
|   ├── utils.h
|   ├── data_utils.h
|   └── table.h
├── src/                         # C source files
│   ├── btree.c
│   ├── command_processor.c
│   ├── input_handling.c
│   ├── queue.c
│   ├── stack.c
│   └── table.c
├── main.c                       # Main entry point for the C program
├── Makefile                     # Build automation script
├── Readme.md                    # Project documentation
├── test_db.py                   # Python script for automated testing
├── .gitignore                   # Git ignore file
└── pyrightconfig.json           # Pyright configuration for type checking
```

---

## Testing

Automated tests are provided to verify the functionality of `INSERT`, `SELECT`, `SELECT BY ID`, `UPDATE`, and `DELETE` commands, as well as the integrity of the B-Tree structure.

### Running the Tests

1. **Ensure Python 3 is Installed:**

   Verify that Python 3 is available on your system.

   ```sh
   python3 --version
   ```

2. **Install pytest:**

   Install the `pytest` package using `pip`.

   ```sh
   pip install pytest
   ```

   For Debian-based distributions, if you encounter issues with `pip`, you can install `pytest` using the package manager:

   ```sh
   sudo apt install python3-pytest
   ```

3. **Execute the Test Script:**

   Run the provided Python test script.

   ```sh
   make test
   ```

4. **Review Test Results:**

   The tests will output detailed results, indicating the success or failure of each test case.

---

## Contributing

Contributions are welcome! Follow these steps to contribute to the project:

1. **Fork the Repository:**

   Click the "Fork" button on the repository page to create your own copy.

2. **Create a Feature Branch:**

   Navigate to your forked repository and create a new branch for your feature.

   ```sh
   git checkout -b feature-name
   ```

3. **Commit Your Changes:**

   Make your changes and commit them with a descriptive message.

   ```sh
   git commit -m "Add a new feature"
   ```

4. **Push to Your Branch:**

   ```sh
   git push origin feature-name
   ```

5. **Open a Pull Request:**

   Navigate to the original repository and open a pull request from your feature branch.

6. **Code Review:**

   Collaborate with maintainers to review and refine your contribution.

---

## Contact

For any questions, suggestions, or feedback, feel free to reach out to the project maintainers:

- **Ahmed8881:** [GitHub Profile](https://github.com/Ahmed8881)
- **hamidriaz1998:** [GitHub Profile](https://github.com/hamidriaz1998)
- **abdulrehmansafdar:** [GitHub Profile](https://github.com/abdulrehmansafdar)
- **SherMuhammad:** [Github Profile](https://github.com/shermuhammadgithub)

---

Happy Coding! 🚀
