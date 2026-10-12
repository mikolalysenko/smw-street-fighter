#!/usr/bin/env python3
"""Compatibility entry point; release tooling lives in dev/."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1] / 'dev/package_native.py'), run_name='__main__')
