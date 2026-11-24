import subprocess
import os

# Путь к исполняемому файлу vtsh
VTSH = os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'vtsh')


def run_vtsh(input_text, timeout=5, env=None):
    """
    Запускает vtsh с заданным вводом и возвращает результат.
    
    Args:
        input_text: текст, который будет передан в stdin vtsh
        timeout: максимальное время выполнения в секундах
        env: переменные окружения (если None, используется текущее)
    
    Returns:
        subprocess.CompletedProcess с полями stdout, stderr, returncode
    """
    return subprocess.run(
        [VTSH],
        input=input_text,
        capture_output=True,
        text=True,
        timeout=timeout,
        env=env
    )