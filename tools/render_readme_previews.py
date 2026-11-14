#!/usr/bin/env python3
"""Render README illustrations: real startup C renderer and sample-data UI layouts.

Requires Pillow and a C compiler; never connects to or flashes a device.
"""
from pathlib import Path
import subprocess
import sys
from render_glass_previews import base, quantize
from render_soft_gray_previews import home, library, files, settings

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/screenshots/lumenos'

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    view=ROOT/'build/readme-host'
    subprocess.run([sys.executable,str(ROOT/'tools/board_source_view.py'),
                    '--board','metalio47','--tests','--output',str(view)],check=True)
    subprocess.run([sys.executable,'tools/metalio_startup_host_test.py',
                    '--output',str(OUT/'startup.png')],cwd=view,check=True)
    for name,draw in [('home',home),('library',library),('files',files),
                      ('settings',settings),('reader',lambda:base(reader=True))]:
        quantize(draw()).save(OUT/f'{name}.png',optimize=True)
        print(OUT/f'{name}.png')

if __name__=='__main__':main()
