#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Explicit opt-in installation of the POSIX SIP runtime in this package.
set -eu
cd -- "$(dirname -- "$0")"
if [ ! -x .venv/bin/python ]; then
    "${COINLINE_PYTHON:-python3}" -m venv .venv
fi
.venv/bin/python -m pip install --disable-pip-version-check -r requirements-sip.txt
.venv/bin/python -c 'import pyVoIP.RTP, pyVoIP.VoIP, pylibsrtp; print("CoinLine SIP runtime is ready.")'
