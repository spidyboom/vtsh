import subprocess
import unittest
import os

#запуск втша захват дескрипторов
VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')

class TestRepl(unittest.TestCase):
    def test_eof_exits_cleanly(self):
        """Проверяем, что vtsh корректно выходит при EOF"""
        # втша без ввода (EOF)
        p = subprocess.run([VTSH], input='', capture_output=True, text=True, timeout=5)
        self.assertEqual(p.returncode, 0)

    def test_echo_line(self):
        """Проверяем, что vtsh печатает то, что ввели"""
        p = subprocess.run([VTSH], input='hello\n', capture_output=True, text=True, timeout=5)
        self.assertIn('hello', p.stdout)

    def test_empty_line(self):
        """Проверяем, что пустая строка не вызывает ошибок"""
        p = subprocess.run([VTSH], input='\n', capture_output=True, text=True, timeout=5)
        self.assertEqual(p.returncode, 0)

    def test_multiple_lines(self):
        """Проверяем, что vtsh обрабатывает несколько строк"""
        p = subprocess.run([VTSH], input='line1\nline2\n', capture_output=True, text=True, timeout=5)
        self.assertIn('line1', p.stdout)
        self.assertIn('line2', p.stdout)

if __name__ == '__main__':
    unittest.main()