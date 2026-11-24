import unittest
import time
from helpers import run_vtsh


class TestBackground(unittest.TestCase):
    """Тесты фонового режима &"""

    def test_bg_does_not_block(self):
        """Фоновая команда не блокирует следующую"""
        p = run_vtsh('/bin/sleep 0.5 &\n/bin/echo done\n', timeout=10)
        self.assertIn('done', p.stdout)

    def test_bg_prints_pid(self):
        """Фоновая команда печатает PID"""
        p = run_vtsh('/bin/sleep 0.1 &\n', timeout=5)
        # Должен быть вывод вида [12345]
        self.assertRegex(p.stdout, r'\[\d+\]')

    def test_bg_no_zombies(self):
        """После фоновой команды не остаётся зомби"""
        # Запускаем несколько фоновых команд и ждём их завершения
        cmds = '\n'.join(['/bin/sleep 0.1 &'] * 5)
        p = run_vtsh(cmds + '\n/bin/sleep 0.3\n', timeout=10)
        self.assertEqual(p.returncode, 0)

    def test_bg_pipeline(self):
        """Фоновый конвейер"""
        p = run_vtsh('/bin/sleep 0.2 | /bin/cat &\n/bin/echo done\n', timeout=10)
        self.assertIn('done', p.stdout)
        self.assertRegex(p.stdout, r'\[\d+\]')

    def test_fg_still_waits(self):
        """Foreground команда всё ещё ждёт"""
        start = time.time()
        p = run_vtsh('/bin/sleep 0.3\n/bin/echo done\n', timeout=5)
        elapsed = time.time() - start
        self.assertIn('done', p.stdout)
        # Должно занять минимум 0.3 секунды
        self.assertGreaterEqual(elapsed, 0.25)


if __name__ == '__main__':
    unittest.main()