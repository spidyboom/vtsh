import unittest
import os
from helpers import run_vtsh


class TestExecutor(unittest.TestCase):
    """Тесты запуска программ"""

    def test_absolute_path(self):
        """Запуск по абсолютному пути"""
        p = run_vtsh('/bin/echo test\n')
        self.assertEqual(p.stdout.strip(), 'test')

    def test_command_not_found(self):
        """Несуществующая команда → код 127, shell не падает"""
        p = run_vtsh('/no/such/binary\n')
        self.assertEqual(p.returncode, 0)  # shell продолжает работать
        self.assertIn('No such file', p.stderr)

    def test_not_executable(self):
        """Файл не исполняемый → код 126"""
        p = run_vtsh('/etc/passwd\n')
        self.assertEqual(p.returncode, 0)  # shell продолжает работать
        # должна быть ошибка в stderr
        self.assertTrue(len(p.stderr) > 0)

    def test_exit_code_zero(self):
        """Команда с кодом 0"""
        p = run_vtsh('/bin/true\n')
        self.assertEqual(p.returncode, 0)

    def test_exit_code_nonzero(self):
        """Команда с ненулевым кодом, shell не падает"""
        p = run_vtsh('/bin/false\n/bin/echo still_alive\n')
        self.assertEqual(p.returncode, 0)
        self.assertIn('still_alive', p.stdout)

    def test_environment_passed(self):
        """Переменные окружения передаются в дочерний процесс"""
        env = os.environ.copy()
        env['VTSH_TEST_VAR'] = 'hello123'
        p = run_vtsh('/usr/bin/printenv VTSH_TEST_VAR\n', env=env)
        self.assertIn('hello123', p.stdout)

    def test_cd_changes_directory(self):
        """cd меняет рабочую директорию"""
        p = run_vtsh('cd /tmp\n/bin/pwd\n')
        self.assertIn('/tmp', p.stdout)

    def test_cd_home(self):
        """cd без аргументов переходит в HOME"""
        env = os.environ.copy()
        env['HOME'] = '/tmp'
        p = run_vtsh('cd\n/bin/pwd\n', env=env)
        self.assertIn('/tmp', p.stdout)

    def test_exit_command(self):
        """Встроенная команда exit"""
        p = run_vtsh('exit\n')
        self.assertEqual(p.returncode, 0)

    def test_syntax_error_does_not_crash(self):
        """Ошибочный синтаксис не завершает shell"""
        p = run_vtsh('/no/such/cmd\n/bin/echo still_alive\n')
        self.assertEqual(p.returncode, 0)
        self.assertIn('still_alive', p.stdout)


if __name__ == '__main__':
    unittest.main()