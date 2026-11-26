import subprocess
import unittest
import os

VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')


def run_vtsh(input_text, timeout=5):
    return subprocess.run(
        [VTSH],
        input=input_text,
        capture_output=True,
        text=True,
        timeout=timeout
    )


class TestPipeline(unittest.TestCase):
    """Тесты конвейеров |"""

    def test_simple_pipe(self):
        """Простой конвейер из двух команд"""
        p = run_vtsh('/bin/echo hello world | /usr/bin/wc -w\n')
        self.assertIn('2', p.stdout)

    def test_pipe_eof_propagation(self):
        """EOF корректно передаётся через конвейер (не зависает)"""
        p = run_vtsh('/bin/echo test | /bin/cat\n')
        self.assertEqual(p.stdout.strip(), 'test')

    def test_three_stage_pipe(self):
        """Конвейер из трёх команд"""
        # echo → wc -w (считает слова) → cat (выводит результат)
        p = run_vtsh('/bin/echo a b c d e | /usr/bin/wc -w | /bin/cat\n')
        self.assertIn('5', p.stdout)

    def test_pipe_status_is_last(self):
        """Статус конвейера = статус последнего звена"""
        # false | true → статус 0 (от true)
        p = run_vtsh('/bin/false | /bin/true\n')
        self.assertEqual(p.returncode, 0)

    def test_pipe_with_or(self):
        """Конвейер в сочетании с ||"""
        p = run_vtsh('/bin/false || /bin/echo ok | /usr/bin/tr a-z A-Z\n')
        self.assertIn('OK', p.stdout)

    def test_pipe_no_hang(self):
        """Конвейер не зависает (проверка через timeout)"""
        # если родитель не закроет концы каналов, wc зависнет
        p = run_vtsh('/bin/echo hello | /usr/bin/wc -c\n', timeout=3)
        self.assertIn('6', p.stdout)  # "hello\n" = 6 байт


if __name__ == '__main__':
    unittest.main()