"""El runner nunca debe confundir un crash o error distinto con un rechazo correcto."""
import subprocess
import unittest
from run_fixtures import validate_result


def result(code=1, diagnostic='SEM008', stderr=''):
    return subprocess.CompletedProcess([], code,
        f'Diagnosticos:\n  [{diagnostic}] error 1:2: ejemplo\n', stderr)


class FixtureRunnerTest(unittest.TestCase):
    def test_expected_diagnostic(self):
        self.assertIsNone(validate_result(result(), ['SEM008']))

    def test_wrong_diagnostic(self):
        self.assertIsNotNone(validate_result(result(diagnostic='SYN001'), ['SEM008']))

    def test_crash_even_with_expected_diagnostic(self):
        self.assertIsNotNone(validate_result(result(code=-11), ['SEM008']))

    def test_uncontrolled_exit(self):
        self.assertIsNotNone(validate_result(result(code=134), ['SEM008']))

    def test_missing_diagnostic(self):
        self.assertIsNotNone(validate_result(subprocess.CompletedProcess([], 1, '', ''), ['SEM008']))

    def test_additional_diagnostic(self):
        proc = result()
        proc.stdout += '  [SEM004] error 2:1: otro\n'
        self.assertIsNotNone(validate_result(proc, ['SEM008']))

    def test_valid_fixture(self):
        self.assertIsNone(validate_result(subprocess.CompletedProcess([], 0, 'AST:\nProgram', ''), []))

    def test_invalid_location(self):
        proc = result()
        proc.stdout = proc.stdout.replace('1:2:', '0:0:')
        self.assertIsNotNone(validate_result(proc, ['SEM008']))

    def test_stderr(self):
        self.assertIsNotNone(validate_result(result(stderr='assertion failed'), ['SEM008']))


if __name__ == '__main__':
    unittest.main()
