# Database Project

## Overview

The **Database Project** is a lightweight, command-line-based database engine implemented in **C**. Inspired by SQLite, it offers fundamental functionalities for managing and querying data using a B-Tree data structure. This project serves as an educational tool to understand the core principles behind database systems, including data storage, indexing, and query processing.

---

## Features

- **Multi-Database Support:** Create and manage multiple databases
- **Table Management:** Create tables with various data types
- **Insert Records:** Add new entries to the database
- **Select Records:** Retrieve and display stored data
- **Select Records by ID:** Retrieve and display a specific record by its ID
- **Update Records:** Modify existing entries in the database
- **Delete Records:** Remove entries from the database
- **B-Tree Indexing:** Efficient data organization and retrieval using B-Trees
- **Command-Line Interface:** Interactive shell for executing SQL-like commands
- **Meta-Commands:** Special commands prefixed with `.` for additional functionalities like viewing the B-Tree structure and application constants

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

   - This command compiles the C source files and generates the executable at `bin/db-project`.

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

   Execute the compiled binary:

   ```sh
   ./bin/db-project
   ```

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

- **Select Data by ID:**

  ```sql
  SELECT * FROM table_name WHERE id = <id>
  ```

  Example:
  ```sql
  SELECT * FROM students WHERE id = 1
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
db > SELECT * FROM students
(1, Alice, 3.8)
(2, Bob, 3.5)
Executed.
db > SELECT * FROM students WHERE id = 1
(1, Alice, 3.8)
Executed.
db > UPDATE students SET name = "Alicia" WHERE id = 1
Executed.
db > SELECT * FROM students WHERE id = 1
(1, Alicia, 3.8)
Executed.
db > DELETE FROM students WHERE id = 1
Executed.
db > SELECT * FROM students
(2, Bob, 3.5)
Executed.
db > .btree
Tree:
- internal (size 1)
  - leaf (size 1)
    - 1
  - key 1
  - leaf (size 1)
    - 2
- key 2
db > .constants
ROW_SIZE: 291
COMMON_NODE_HEADER_SIZE: 6
LEAF_NODE_HEADER_SIZE: 14
LEAF_NODE_CELL_SIZE: 292
LEAF_NODE_SPACE_FOR_CELLS: 4082
LEAF_NODE_MAX_CELLS: 13
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
