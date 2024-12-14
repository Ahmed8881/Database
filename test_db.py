import unittest
import subprocess
import os

class TestDatabase(unittest.TestCase):

    def run_script(self, commands, program="./bin/db-project"):
        process = subprocess.Popen([program, "test.db"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        for command in commands:
            process.stdin.write(command + "\n")
        process.stdin.close()
        raw_output = process.stdout.read()
        process.stdout.close()
        process.wait()
        return raw_output.split("\n")

    def test_inserts_and_retrieves_a_row(self):
        result = self.run_script([
            "insert 1 user1 person1@example.com",
            "select",
            ".exit",
        ])
        self.assertEqual(result, [
            "db > Executed.",
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ])
        os.remove("test.db")

    def test_prints_error_message_when_table_is_full(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 1402)]
        script.append(".exit")
        result = self.run_script(script)
        self.assertEqual(result[-2], 'db > Error: Table full.')
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
        self.assertEqual(result, [
            "db > Executed.",
            f"db > (1, {long_username}, {long_email})",
            "Executed.",
            "db > ",
        ])
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
        self.assertEqual(result, [
            "db > String is too long.",
            "db > Executed.",
            "db > ",
        ])
        os.remove("test.db")

    def test_prints_an_error_message_if_id_is_negative(self):
        script = [
            "insert -1 cstack foo@bar.com",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        self.assertEqual(result, [
            "db > ID must be positive.",
            "db > Executed.",
            "db > ",
        ])
        os.remove("test.db")

    def test_keeps_data_after_closing_connection(self):
        result1 = self.run_script([
            "insert 1 user1 person1@example.com",
            ".exit",
        ])
        self.assertEqual(result1, [
            "db > Executed.",
            "db > ",
        ])
        result2 = self.run_script([
            "select",
            ".exit",
        ])
        self.assertEqual(result2, [
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ])
        os.remove("test.db")
    def test_prints_constants(self):
        script = [
            ".constants",
            ".exit",
        ]
        result = self.run_script(script)
        self.assertEqual(result, [
            "db > Constants:",
            "ROW_SIZE: 293",
            "COMMON_NODE_HEADER_SIZE: 6",
            "LEAF_NODE_HEADER_SIZE: 10",
            "LEAF_NODE_CELL_SIZE: 297",
            "LEAF_NODE_SPACE_FOR_CELLS: 4086",
            "LEAF_NODE_MAX_CELLS: 13",
            "db > ",
        ])
        os.remove("test.db")
    def test_allows_printing_out_the_structure_of_a_one_node_btree(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in [3, 1, 2]]
        script.append(".btree")
        script.append(".exit")
        result = self.run_script(script)
        self.assertEqual(result, [
            "db > Executed.",
            "db > Executed.",
            "db > Executed.",
            "db > Tree:",
            "leaf (size 3)",
            "  - 0 : 1",
            "  - 1 : 2",
            "  - 2 : 3",
            "db > ",
        ])
        os.remove("test.db")
    def test_prints_an_error_message_if_there_is_a_duplicate_id(self):
        script = [
            "insert 1 user1 person1@example.com",
            "insert 1 user1 person1@example.com",
            "select",
            ".exit",
        ]
        result = self.run_script(script)
        self.assertEqual(result, [
            "db > Executed.",
            "db > Error: Duplicate key.",
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ])
        os.remove("test.db")
if __name__ == '__main__':
    unittest.main()
