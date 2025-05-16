import subprocess

def run_script(commands, program="./bin/db-project"):
    process = subprocess.Popen([program, "test.db"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        for command in commands:
            process.stdin.write(command + "\n")
        process.stdin.close()
        raw_output = process.stdout.read()
        process.stdout.close()
        process.wait()
        return raw_output.split("\n")
    except BrokenPipeError:
        raw_output = process.stdout.read()
        process.stdout.close()
        process.wait()
        return raw_output.split("\n")

def insert_dummy_data():
    commands = [
        "create database school",
        "use database school",
        "create table students (id INT, name STRING, father_name STRING, gpa FLOAT, age INT, gender STRING)",
        "use table students",
        "insert into students values (1, 'John Doe', 'Richard Roe', 3.5, 20, 'M')",
        "insert into students values (2, 'Jane Smith', 'John Smith', 3.8, 22, 'F')",
        "insert into students values (3, 'Alice Johnson', 'Robert Johnson', 3.2, 19, 'F')",
        "insert into students values (4, 'Bob Brown', 'Michael Brown', 3.9, 21, 'M')",
        "insert into students values (5, 'Charlie Davis', 'David Davis', 3.6, 23, 'M')",
        "insert into students values (6, 'Diana Evans', 'William Evans', 3.7, 20, 'F')",
        "insert into students values (7, 'Ethan Wilson', 'James Wilson', 3.4, 22, 'M')",
        "insert into students values (8, 'Fiona Taylor', 'Charles Taylor', 3.1, 19, 'F')",
        "insert into students values (9, 'George Anderson', 'Joseph Anderson', 3.3, 21, 'M')",
    ]
    try:
        output = run_script(commands)
        for line in output:
            print(line)
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    insert_dummy_data()