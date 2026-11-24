import unittest
from helpers import run_vtsh


class TestPipeline(unittest.TestCase):
    """Тесты конвейеров |"""

    def test_simple_pipe(self):
        """Простой конвейер из двух команд"""
        p = run_vtsh('/bin/echo hello world | /usr/bin/wc -w\n')
        self.assertIn('2', p.stdout)

    def test_pipe_eof_propagation(self):
        """Конвейер корректно передаёт EOF (не зависает)"""
        p = run_vtsh('/bin/echo test | /bin/cat\n')
        self.assertEqual(p.stdout.strip(), 'test')

    def test_three_stage_pipe(self):
        """Конвейер из трёх команд (без кавычек, так как парсер их не поддерживает)"""
        p = run_vtsh('/bin/echo 1 2 3 | /usr/bin/tr 1 4 | /usr/bin/wc -w\n')
        self.assertIn('3', p.stdout)

    def test_pipe_with_or(self):
        """Конвейер в сочетании с ||"""
        p = run_vtsh('/bin/false || /bin/echo ok | /usr/bin/tr "a-z" "A-Z"\n')
        self.assertIn('OK', p.stdout)

    def test_pipe_first_fails(self):
        """Ошибка в первом звене конвейера"""
        p = run_vtsh('/no/such/cmd | /bin/echo fallback\n')
        self.assertIn('No such file', p.stderr)
        self.assertIn('fallback', p.stdout)

    def test_pipe_count_bytes(self):
        """Подсчёт байт через конвейер"""
        p = run_vtsh('/bin/echo hello | /usr/bin/wc -c\n')
        # "hello\n" = 6 байт
        self.assertIn('6', p.stdout)

    def test_pipe_sort(self):
        """Сортировка через конвейер (без кавычек, используем printf с множественными аргументами)"""
        p = run_vtsh('/bin/printf %s\\n c a b | /usr/bin/sort\n')
        self.assertEqual(p.stdout.strip(), 'a\nb\nc')


if __name__ == '__main__':
    unittest.main()