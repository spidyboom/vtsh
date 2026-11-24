import unittest
from helpers import run_vtsh


class TestRepl(unittest.TestCase):
    """Тесты базового цикла REPL"""

    def test_eof_exits_cleanly(self):
        """При EOF (Ctrl+D) shell выходит с кодом 0"""
        p = run_vtsh('')
        self.assertEqual(p.returncode, 0)

    def test_empty_line(self):
        """Пустая строка не вызывает ошибок"""
        p = run_vtsh('\n')
        self.assertEqual(p.returncode, 0)

    def test_multiple_empty_lines(self):
        """Несколько пустых строк подряд"""
        p = run_vtsh('\n\n\n')
        self.assertEqual(p.returncode, 0)

    def test_multiple_lines(self):
        """Несколько команд подряд выполняются"""
        p = run_vtsh('/bin/echo first\n/bin/echo second\n')
        self.assertIn('first', p.stdout)
        self.assertIn('second', p.stdout)


if __name__ == '__main__':
    unittest.main()