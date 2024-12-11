# Database Project

## Overview
This project implements a SQLite-like database in **C**, designed to run from the command line (CMD). It provides a lightweight, self-contained solution for executing basic SQL commands to manage data.

---

## Features
- **Insert Records:** Add data to the database.
- **Select Records:** Query and retrieve data from the database.
- **SQL Command Handling:** Execute basic SQL commands like `INSERT` and `SELECT`.

---

## Getting Started

### Prerequisites
Before you begin, ensure you have the following tools installed on your system:
- **GCC (GNU Compiler Collection):** For compiling the source code.
- **Python 3:** Required for running the test scripts.

---

### Installation

1. **Clone the repository:**
   ```sh
   git clone https://github.com/Ahmed8881/Database.git
   cd Database
   ```

2. **Compile the source code:**
   ```sh
   gcc -o database database.c
   ```

3. **Run the application:**
   ```sh
   ./database
   ```

---

## Usage

### Supported SQL Commands
The database currently supports a subset of SQL commands:
- **Insert Data:**
  ```sql
  INSERT INTO table_name (column1, column2) VALUES (value1, value2);
  ```
- **Select Data:**
  ```sql
  SELECT column1, column2 FROM table_name WHERE condition;
  ```

### Example Usage
1. **Start the application:**
   ```sh
   ./database
   ```

2. **Insert a record:**
   ```sql
   INSERT INTO users (id, name) VALUES (1, 'Alice');
   ```

3. **Select records:**
   ```sql
   SELECT id, name FROM users;
   ```

---

## Project Structure
The project files are organized as follows:
```
Database/
├── __pycache__/          # Compiled Python files
├── include/              # Header files for the C source code
│   ├── command_processor.h
│   ├── input_handling.h
│   └── table.h
├── src/                  # C source files
│   ├── command_processor.c
│   ├── input_handling.c
│   └── table.c
├── main.c                # Main entry point for the C program
├── Makefile              # Build automation script
├── Readme.md             # Project documentation
├── tests.py              # Python script for automated testing
└── .gitignore            # Git ignore file

```

---

## Testing

1. **Run the test script:**
   Make sure Python 3 is installed, then run:
   ```sh
   python3 test_script.py
   ```

2. **Expected output:**
   - The script verifies the functionality of `INSERT` and `SELECT` commands.
   - Detailed test results will be displayed in the terminal.

---

## Contributing

Contributions are welcome! To get started:

1. Fork the repository.
2. Create a feature branch:
   ```sh
   git checkout -b feature-name
   ```
3. Commit your changes:
   ```sh
   git commit -m "Add a new feature"
   ```
4. Push to your branch:
   ```sh
   git push origin feature-name
   ```
5. Open a Pull Request.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## Contact

If you have any questions or suggestions, feel free to reach out:
- **GitHub:** [Ahmed8881](https://github.com/Ahmed8881)
- **GitHub:** [hamidriaz1998](https://github.com/hamidriaz1998)
- **GitHub:** [abdulrehmansafdar](https://github.com/abdulrehmansafdar)

---

Happy Coding! 🚀