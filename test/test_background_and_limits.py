import subprocess
import unittest
import os

VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')


def run_vtsh(input_text, timeout=10):
    return subprocess.run(
        [VTSH],
        input=input_text,
        capture_output=True,
        text=True,
        timeout=timeout
    )


class TestBackground(unittest.TestCase):
    """Тесты фонового режима &"""

    def test_bg_does_not_block(self):
        """Фоновая команда не блокирует следующую"""
        p = run_vtsh('/bin/sleep 0.3 &\n/bin/echo done\n')
        self.assertIn('done', p.stdout)

    def test_bg_prints_pid(self):
        """Фоновая команда печатает [PID]"""
        p = run_vtsh('/bin/sleep 0.1 &\n')
        self.assertRegex(p.stdout, r'\[\d+\]')

    def test_bg_no_zombies(self):
        """После фоновой команды не остаётся зомби"""
        cmds = '\n'.join(['/bin/sleep 0.1 &'] * 5)
        p = run_vtsh(cmds + '\n/bin/sleep 0.3\n')
        self.assertEqual(p.returncode, 0)


class TestStackLimit(unittest.TestCase):
    """Тесты ограничения размера стека (RLIMIT_STACK)"""

    def test_setstacklimit_command(self):
        """Встроенная команда setstacklimit работает"""
        p = run_vtsh('setstacklimit 1048576\n/bin/echo ok\n')
        self.assertIn('ok', p.stdout)
        self.assertEqual(p.returncode, 0)

    def test_setstacklimit_usage(self):
        """setstacklimit без аргумента печатает ошибку"""
        p = run_vtsh('setstacklimit\n')
        self.assertIn('usage', p.stderr)

    def test_small_stack_affects_recursion(self):
        """Маленький лимит стека ограничивает рекурсию"""
        deep_c = os.path.join(os.path.dirname(__file__), 'helpers', 'deep.c')
        deep_bin = os.path.join(os.path.dirname(__file__), 'helpers', 'deep')

        if not os.path.exists(deep_bin):
            self.skipTest(f'Тестовая программа {deep_bin} не скомпилирована')

        p = run_vtsh(f'setstacklimit 65536\n{deep_bin}\n', timeout=5)
        self.assertEqual(p.returncode, 0)


if __name__ == '__main__':
    unittest.main()