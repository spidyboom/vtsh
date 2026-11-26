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


class TestLogicalOr(unittest.TestCase):
    """Тесты оператора ||"""

    def test_or_runs_second_on_failure(self):
        """Вторая команда выполняется, если первая упала"""
        p = run_vtsh('/bin/false || /bin/echo fallback\n')
        self.assertIn('fallback', p.stdout)

    def test_or_skips_second_on_success(self):
        """Вторая команда НЕ выполняется, если первая успешна"""
        p = run_vtsh('/bin/true || /bin/echo bad\n')
        self.assertNotIn('bad', p.stdout)

    def test_or_chain_all_fail(self):
        """Цепочка || — все падают, последняя выполняется"""
        p = run_vtsh('/bin/false || /bin/false || /bin/echo recovered\n')
        self.assertIn('recovered', p.stdout)

    def test_or_chain_stops_on_success(self):
        """Цепочка || — останавливается на первой успешной"""
        p = run_vtsh('/bin/false || /bin/true || /bin/echo bad\n')
        self.assertNotIn('bad', p.stdout)

    def test_or_with_command_not_found(self):
        """Несуществующая команда считается ошибкой (код 127)"""
        p = run_vtsh('/no/such/cmd || /bin/echo fallback\n')
        self.assertIn('fallback', p.stdout)

    def test_or_lazy_evaluation(self):
        """Ленивое вычисление: если первая успешна, вторая не создаёт процесс"""
        p = run_vtsh('/bin/true || /bin/echo bad\n')
        self.assertNotIn('bad', p.stdout)
        self.assertEqual(p.returncode, 0)


if __name__ == '__main__':
    unittest.main()