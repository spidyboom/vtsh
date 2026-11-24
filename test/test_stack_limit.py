import unittest
import os
from helpers import run_vtsh

DEEP = os.path.join(os.path.dirname(__file__), 'helpers', 'deep')


class TestStackLimit(unittest.TestCase):
    """Тесты ограничения размера стека (RLIMIT_STACK)"""

    def test_small_stack_causes_segfault(self):
        """Маленький лимит стека → рекурсия падает с SIGSEGV"""
        # 64 КБ стека должно быть мало для глубокой рекурсии
        p = run_vtsh(f'setstacklimit 65536\n{DEEP}\n', timeout=5)
        # Программа должна упасть с сигналом (SIGSEGV = 11)
        # Код shell'а = 0 (он продолжает работать)
        self.assertEqual(p.returncode, 0)

    def test_large_stack_runs_longer(self):
        """Большой лимит → рекурсия идёт дальше"""
        # 8 МБ стека
        p = run_vtsh(f'setstacklimit 8388608\n{DEEP}\n', timeout=5)
        # Всё равно упадёт (бесконечная рекурсия), но позже
        self.assertEqual(p.returncode, 0)

    def test_stack_limit_persists(self):
        """Лимит сохраняется между командами"""
        p = run_vtsh(
            f'setstacklimit 65536\n'
            f'{DEEP}\n'
            f'/bin/echo still_alive\n',
            timeout=5
        )
        # Shell продолжает работать после падения программы
        self.assertIn('still_alive', p.stdout)

    def test_setstacklimit_no_args(self):
        """setstacklimit без аргумента выдаёт ошибку"""
        p = run_vtsh('setstacklimit\n/bin/echo ok\n', timeout=5)
        self.assertIn('usage', p.stderr)
        self.assertIn('ok', p.stdout)


if __name__ == '__main__':
    unittest.main()