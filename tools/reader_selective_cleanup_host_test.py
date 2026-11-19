#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compatibility entry: retired selective GC/GL37 fallback is replaced by GL48."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('reader_native48_host_test.py')),run_name='__main__')
