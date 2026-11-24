import unittest
from helpers import run_vtsh


class TestLogicalOr(unittest.TestCase):
    """Тесты оператора ||"""

    def test_or_runs_second_on_failure(self):
        """Вторая команда выполняется если первая упала"""
        p = run_vtsh('/bin/false || /bin/echo fallback\n')
        self.assertIn('fallback', p.stdout)

    def test_or_skips_second_on_success(self):
        """Вторая команда НЕ выполняется если первая успешна"""
        p = run_vtsh('/bin/true || /bin/echo fallback\n')
        self.assertNotIn('fallback', p.stdout)

    def test_or_chain_all_fail(self):
        """Цепочка || — все падают, последняя выполняется"""
        p = run_vtsh('/bin/false || /bin/false || /bin/echo recovered\n')
        self.assertIn('recovered', p.stdout)

    def test_or_chain_stops_on_success(self):
        """Цепочка || — останавливается на первой успешной"""
        p = run_vtsh('/bin/false || /bin/true || /bin/echo bad\n')
        self.assertNotIn('bad', p.stdout)

    def test_or_with_command_not_found(self):
        """|| с несуществующей командой (код 127)"""
        p = run_vtsh('/no/such/command || /bin/echo fallback\n')
        self.assertIn('fallback', p.stdout)
        self.assertIn('No such file', p.stderr)

    def test_or_lazy_evaluation(self):
        """Ленивое вычисление: вторая команда не создаёт процесс"""
        # Если бы вторая команда создалась, она бы вывела "bad"
        p = run_vtsh('/bin/true || /bin/echo bad\n')
        self.assertNotIn('bad', p.stdout)


if __name__ == '__main__':
    unittest.main()