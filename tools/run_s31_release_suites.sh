#!/usr/bin/env bash
# 中文：S31 发布前主机测试关口，见 tools/run_s31_release_suites.py。/ English: S31 release host-test gate; see tools/run_s31_release_suites.py.
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 tools/run_s31_release_suites.py "$@"
