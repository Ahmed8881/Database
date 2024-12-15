import subprocess
import os

class TestDatabase:

    def run_script(self, commands, program="./bin/db-project"):
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

    def test_inserts_and_retrieves_a_row(self):
        if os.path.exists("test.db"):
            os.remove("test.db")
        result = self.run_script([
            "insert 1 user1 person1@example.com",
            "select",
            ".exit",
        ])
        assert result == [
            "db > Executed.",
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_prints_error_message_when_table_is_full(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 1402)]
        script.append(".exit")
        result = self.run_script(script)
        assert result[-2] == 'db > Tried to fetch page out of bounds. 101 > 100'
        os.remove("test.db")

    def test_allows_inserting_strings_that_are_the_maximum_length(self):
        long_username = "a" * 32
        long_email = "a" * 255
        script = [
            f"insert 1 {long_username} {long_email}",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        assert result == [
            "db > Executed.",
            f"db > (1, {long_username}, {long_email})",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_prints_error_message_if_strings_are_too_long(self):
        long_username = "a" * 33
        long_email = "a" * 256
        script = [
            f"insert 1 {long_username} {long_email}",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        assert result == [
            "db > String is too long.",
            "db > Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_prints_an_error_message_if_id_is_negative(self):
        script = [
            "insert -1 cstack foo@bar.com",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        assert result == [
            "db > ID must be positive.",
            "db > Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_keeps_data_after_closing_connection(self):
        result1 = self.run_script([
            "insert 1 user1 person1@example.com",
            ".exit",
        ])
        assert result1 == [
            "db > Executed.",
            "db > ",
        ]
        result2 = self.run_script([
            "select",
            ".exit",
        ])
        assert result2 == [
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_prints_constants(self):
        script = [
            ".constants",
            ".exit",
        ]
        result = self.run_script(script)
        assert result == [
            "db > Constants:",
            "ROW_SIZE: 293",
            "COMMON_NODE_HEADER_SIZE: 6",
            "LEAF_NODE_HEADER_SIZE: 14",
            "LEAF_NODE_CELL_SIZE: 297",
            "LEAF_NODE_SPACE_FOR_CELLS: 4082",
            "LEAF_NODE_MAX_CELLS: 13",
            "db > ",
        ]
        os.remove("test.db")

    def test_allows_printing_out_the_structure_of_a_one_node_btree(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in [3, 1, 2]]
        script.append(".btree")
        script.append(".exit")
        result = self.run_script(script)
        assert result == [
            "db > Executed.",
            "db > Executed.",
            "db > Executed.",
            "db > Tree:",
            "- leaf (size 3)",
            "  - 1",
            "  - 2",
            "  - 3",
            "db > ",
        ]
        os.remove("test.db")

    def test_prints_an_error_message_if_there_is_a_duplicate_id(self):
        script = [
            "insert 1 user1 person1@example.com",
            "insert 1 user1 person1@example.com",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        assert result == [
            "db > Executed.",
            "db > Error: Duplicate key.",
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_allows_printing_out_the_structure_of_a_3_leaf_node_btree(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 15)]
        script.append(".btree")
        script.append("insert 15 user15 person15@example.com")
        script.append(".exit")
        result = self.run_script(script)
        assert result[14:len(result)] == [
        "db > Tree:",
        "- internal (size 1)",
        "  - leaf (size 7)",
        "    - 1",
        "    - 2",
        "    - 3",
        "    - 4",
        "    - 5",
        "    - 6",
        "    - 7",
        "  - key 7",
        "  - leaf (size 7)",
        "    - 8",
        "    - 9",
        "    - 10",
        "    - 11",
        "    - 12",
        "    - 13",
        "    - 14",
        "db > Executed.",
        "db > "
        ]
        os.remove("test.db")

    def test_prints_all_rows_in_multi_level_tree(self):
            script = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 16)]
            script.append("select")
            script.append(".exit")

            result = self.run_script(script)

            expected_inserts_output = ["db > Executed."] * 15

            expected_select_output = ["db > (1, user1, person1@example.com)"]
            expected_select_output += [f"({i}, user{i}, person{i}@example.com)" for i in range(2, 16)]
            expected_select_output += ["Executed.", "db > "]

            expected_output = expected_inserts_output + expected_select_output

            assert result == expected_output, f"Expected output does not match actual output.\nExpected: {expected_output}\nActual: {result}"

            os.remove("test.db")
    def test_select_where_id(self):
        script =[f"insert {i} user{i} person{i}@gmail.com" for i in range(1, 6)]
        script.append("select where id = 3")
        script.append(".exit")
        result = self.run_script(script)
        assert result == [
            "db > Executed.",
            "db > Executed.",
            "db > Executed.",
            "db > Executed.",
            "db > Executed.",
            "db > (3, user3, person3@example.com)",
            "Executed.",
            "db > ",
        ]
        
        os.remove("test.db")
    def test_update_username_where_id(self):
        if os.path.exists("test.db"):
            os.remove("test.db")
        result = self.run_script([
            "insert 1 user1 user1@example.com",
            "update 1 username new_user1",
            "select where id = 1",
            ".exit",
        ])
        assert result == [
            "db > Executed.",
            "db > Executed.",
            "db > (1, new_user1, user1@example.com)",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_update_email_where_id(self):
        if os.path.exists("test.db"):
            os.remove("test.db")
        result = self.run_script([
            "insert 1 user1 user1@example.com",
            "update 1 email new_user1@example.com",
            "select where id = 1",
            ".exit",
        ])
        assert result == [
            "db > Executed.",
            "db > Executed.",
            "db > (1, user1, new_user1@example.com)",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")

    def test_delete_where_id(self):
        if os.path.exists("test.db"):
            os.remove("test.db")
        result = self.run_script([
            "insert 1 user1 user1@example.com",
            "delete where id = 1",
            "select where id = 1",
            ".exit",
        ])
        assert result == [
            "db > Executed.",
            "db > Executed.",
            "db > Record not found.",
            "Executed.",
            "db > ",
        ]
        os.remove("test.db")


