import subprocess
import unittest
import os

VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')


def run_vtsh(input_text, env=None, timeout=5):
    """Запускает vtsh с заданным вводом и возвращает результат"""
    return subprocess.run(
        [VTSH],
        input=input_text,
        capture_output=True,
        text=True,
        timeout=timeout,
        env=env
    )


class TestRepl(unittest.TestCase):
    """Тесты цикла REPL"""

    def test_eof_exits_cleanly(self):
        """EOF (Ctrl+D) завершает shell без ошибок"""
        p = run_vtsh('')
        self.assertEqual(p.returncode, 0)

    def test_empty_line_ignored(self):
        """Пустая строка не вызывает ошибок"""
        p = run_vtsh('\n\n\n')
        self.assertEqual(p.returncode, 0)

    def test_multiple_lines(self):
        """Несколько команд выполняются по очереди"""
        p = run_vtsh('/bin/echo first\n/bin/echo second\n')
        self.assertIn('first', p.stdout)
        self.assertIn('second', p.stdout)


class TestExecutor(unittest.TestCase):
    """Тесты запуска команд"""

    def test_echo_simple(self):
        """Простая команда с аргументами"""
        p = run_vtsh('/bin/echo hello world\n')
        self.assertEqual(p.stdout.strip(), 'hello world')

    def test_command_not_found(self):
        """Несуществующая команда возвращает 127"""
        p = run_vtsh('/no/such/binary\n')
        self.assertEqual(p.returncode, 0)
        self.assertIn('No such file', p.stderr)

    def test_not_executable(self):
        """Файл без прав на выполнение возвращает 126"""
        p = run_vtsh('/etc/passwd\n')
        self.assertEqual(p.returncode, 0)
        self.assertIn('Permission denied', p.stderr)

    def test_exit_code_zero(self):
        """Команда /bin/true возвращает 0"""
        p = run_vtsh('/bin/true\n')
        self.assertEqual(p.returncode, 0)

    def test_exit_code_nonzero(self):
        """Команда /bin/false возвращает не 0"""
        p = run_vtsh('/bin/false\n')
        self.assertEqual(p.returncode, 0)

    def test_environment_passed(self):
        """Переменные окружения передаются в дочерний процесс"""
        env = os.environ.copy()
        env['VTSH_TEST_VAR'] = 'hello123'
        p = run_vtsh('/usr/bin/printenv VTSH_TEST_VAR\n', env=env)
        self.assertIn('hello123', p.stdout)


class TestBuiltins(unittest.TestCase):
    """Тесты встроенных команд"""

    def test_cd(self):
        """cd меняет текущую директорию"""
        p = run_vtsh('cd /tmp\n/bin/pwd\n')
        self.assertIn('/tmp', p.stdout)

    def test_cd_home(self):
        """cd без аргументов переходит в HOME"""
        env = os.environ.copy()
        env['HOME'] = '/tmp'
        p = run_vtsh('cd\n/bin/pwd\n', env=env)
        self.assertIn('/tmp', p.stdout)

    def test_exit_command(self):
        """Встроенная команда exit завершает shell"""
        p = run_vtsh('exit\n')
        self.assertEqual(p.returncode, 0)


if __name__ == '__main__':
    unittest.main()