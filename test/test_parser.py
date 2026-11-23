import subprocess
import unittest
import os

VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')


class TestParser(unittest.TestCase):
    """Тесты парсера через вывод vtsh"""
    
    def run_vtsh(self, input_text):
        """Запускает vtsh с заданным вводом и возвращает stdout"""
        p = subprocess.run(
            [VTSH],
            input=input_text,
            capture_output=True,
            text=True,
            timeout=5
        )
        return p
    
    def test_simple_command(self):
        """Простая команда с аргументами"""
        p = self.run_vtsh('/bin/echo hello world\n')
        self.assertIn('[parsed]', p.stdout)
        self.assertIn('[/bin/echo]', p.stdout)
        self.assertIn('[hello]', p.stdout)
        self.assertIn('[world]', p.stdout)
    
    def test_multiple_spaces(self):
        """Несколько пробелов между словами"""
        p = self.run_vtsh('   /bin/echo    a    b   \n')
        self.assertIn('[/bin/echo]', p.stdout)
        self.assertIn('[a]', p.stdout)
        self.assertIn('[b]', p.stdout)
        # не должно быть пустых аргументов
        self.assertNotIn('[]', p.stdout)
    
    def test_tabs(self):
        """Табы как разделители"""
        p = self.run_vtsh('/bin/echo\ta\tb\n')
        self.assertIn('[/bin/echo]', p.stdout)
        self.assertIn('[a]', p.stdout)
        self.assertIn('[b]', p.stdout)
    
    def test_empty_line(self):
        """Пустая строка не даёт parsed"""
        p = self.run_vtsh('\n')
        self.assertNotIn('[parsed]', p.stdout)
    
    def test_many_arguments(self):
        """Много аргументов (>8, чтобы проверить realloc)"""
        args = ' '.join([f'arg{i}' for i in range(20)])
        p = self.run_vtsh(f'/bin/echo {args}\n')
        self.assertIn('argc=21', p.stdout)  # echo + 20 аргументов
        self.assertIn('[arg19]', p.stdout)
    
    def test_long_line(self):
        """Очень длинная строка"""
        long_arg = 'x' * 10000
        p = self.run_vtsh(f'/bin/echo {long_arg}\n')
        self.assertIn('[parsed]', p.stdout)
        self.assertIn(f'[{long_arg}]', p.stdout)
    
    def test_multiple_lines(self):
        """Несколько команд подряд"""
        p = self.run_vtsh('/bin/echo a\n/bin/echo b\n')
        # должно быть два [parsed]
        self.assertEqual(p.stdout.count('[parsed]'), 2)


if __name__ == '__main__':
    unittest.main()