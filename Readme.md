# Database Project

## Overview

The **Database Project** is a lightweight, command-line-based database engine implemented in **C**. Inspired by SQLite, it offers fundamental functionalities for managing and querying data using a B-Tree data structure. This project serves as an educational tool to understand the core principles behind database systems, including data storage, indexing, and query processing.

---

## Features

- **Insert Records:** Add new entries to the database.
- **Select Records:** Retrieve and display stored data.
- **B-Tree Indexing:** Efficient data organization and retrieval using B-Trees.
- **Command-Line Interface:** Interactive shell for executing SQL-like commands.
- **Meta-Commands:** Special commands prefixed with `.` for additional functionalities like viewing the B-Tree structure and application constants.

---

## Getting Started

### Prerequisites

Ensure you have the following tools installed on your system:

- **GCC (GNU Compiler Collection):** For compiling the source code.
- **Make:** For build automation.
- **Python 3:** Required for running the test scripts.

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

3. **Clean Build Artifacts (Optional):**

   To remove compiled objects and binaries, run:

   ```sh
   make clean
   ```

4. **Run the Application:**

   Execute the compiled binary, specifying a database file. If the file does not exist, it will be created.

   ```sh
   ./bin/db-project <database_file>
   ```

   _Example:_

   ```sh
   ./bin/db-project test.db
   ```

---

## Usage

Upon running the application, you'll enter an interactive shell where you can execute SQL-like commands and meta-commands.

### Supported SQL Commands

The database currently supports a subset of SQL commands:

- **Insert Data:**

  ```sql
  insert <id> <username> <email>
  ```

  - **`<id>`:** A positive integer representing the user's ID.
  - **`<username>`:** A string up to 32 characters.
  - **`<email>`:** A string up to 255 characters.

  _Example:_

  ```sql
  insert 1 alice alice@example.com
  ```

- **Select Data:**

  ```sql
  select
  ```

  - Retrieves and displays all records in the database.

  _Example:_

  ```sql
  select
  ```

### Supported Meta-Commands

Meta-commands provide additional functionalities and are prefixed with a dot (`.`).

- **Exit Application:**

  ```sh
  .exit
  ```

  - Safely closes the database and exits the application.

- **View B-Tree Structure:**

  ```sh
  .btree
  ```

  - Displays the current structure of the B-Tree, showing internal and leaf nodes with their respective keys.

- **View Application Constants:**

  ```sh
  .constants
  ```

  - Prints out various constants used within the application, such as sizes and offsets related to node structures.

_Note:_ Entering an unrecognized meta-command will result in an error message.

### Example Session

```sh
db > insert 1 alice alice@example.com
Executed.
db > insert 2 bob bob@example.com
Executed.
db > select
(1, alice, alice@example.com)
(2, bob, bob@example.com)
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
├── include/                     # Header files for the C source code
│   ├── btree.h
│   ├── command_processor.h
│   ├── cursor.h
│   ├── input_handling.h
│   ├── pager.h
│   └── table.h
├── src/                         # C source files
│   ├── btree.c
│   ├── command_processor.c
│   ├── input_handling.c
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

Automated tests are provided to verify the functionality of `INSERT` and `SELECT` commands, as well as the integrity of the B-Tree structure.

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

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.

---

## Contact

For any questions, suggestions, or feedback, feel free to reach out to the project maintainers:

- **Ahmed8881:** [GitHub Profile](https://github.com/Ahmed8881)
- **hamidriaz1998:** [GitHub Profile](https://github.com/hamidriaz1998)
- **abdulrehmansafdar:** [GitHub Profile](https://github.com/abdulrehmansafdar)

---

Happy Coding! 🚀
