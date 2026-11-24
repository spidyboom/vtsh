import unittest
from helpers import run_vtsh


class TestParser(unittest.TestCase):
    """Тесты парсера: обработка пробелов, аргументов и т.д."""

    def test_simple_command(self):
        """Простая команда с аргументами"""
        p = run_vtsh('/bin/echo hello world\n')
        self.assertEqual(p.stdout.strip(), 'hello world')

    def test_multiple_spaces(self):
        """Несколько пробелов между словами"""
        p = run_vtsh('/bin/echo    a    b    c\n')
        self.assertEqual(p.stdout.strip(), 'a b c')

    def test_leading_trailing_spaces(self):
        """Пробелы в начале и конце"""
        p = run_vtsh('   /bin/echo hello   \n')
        self.assertEqual(p.stdout.strip(), 'hello')

    def test_tabs(self):
        """Табы как разделители"""
        p = run_vtsh('/bin/echo\ta\tb\n')
        self.assertEqual(p.stdout.strip(), 'a b')

    def test_many_arguments(self):
        """Много аргументов (>8, чтобы проверить realloc)"""
        args = ' '.join([f'arg{i}' for i in range(20)])
        p = run_vtsh(f'/bin/echo {args}\n')
        self.assertIn('arg0', p.stdout)
        self.assertIn('arg19', p.stdout)

    def test_command_no_args(self):
        """Команда без аргументов"""
        p = run_vtsh('/bin/pwd\n')
        self.assertEqual(p.returncode, 0)
        self.assertTrue(len(p.stdout.strip()) > 0)


if __name__ == '__main__':
    unittest.main()