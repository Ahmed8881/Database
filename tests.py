import unittest
import subprocess

class TestDatabase(unittest.TestCase):

    def run_script(self, commands):
        process = subprocess.Popen(["bin/db-project"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
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

    def test_prints_error_message_when_table_is_full(self):
        script = [f"insert {i} user{i} person{i}@example.com" for i in range(1, 1402)]
        script.append(".exit")
        result = self.run_script(script)
        self.assertEqual(result[-2], 'db > Error: Table full.')

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

if __name__ == '__main__':
    unittest.main()
