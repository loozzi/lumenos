#!/usr/bin/env bash
# Run in a Metalio47 host-test view.
set -euo pipefail
mkdir -p build
python3 tools/lumen_branding_host_test.py
python3 tools/metalio_startup_host_test.py --output build/lumen-startup.png
python3 tools/transfer_open_ap_host_test.py
