import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_production_download_trace_and_route_options_are_available():
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools/acceptance/graphic_session.py"), "--help"],
        check=True,
        capture_output=True,
        text=True,
    )
    for option in ("--flash-trace", "--ncc-port", "--code-server-number", "--code-server-port"):
        assert option in result.stdout
