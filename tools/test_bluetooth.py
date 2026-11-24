#!/usr/bin/env python3
"""Run the production Realtek firmware parser and HCI/fragment checks on a host."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
from embed_bt_firmware import SHA256

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler', default=os.environ.get('CXX'))
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--runtime-path')
    args = p.parse_args()
    compiler = args.compiler or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        p.error('C++17 compiler required')
    root = Path(__file__).resolve().parent.parent
    firmware = root / 'firmware/rtl_bt/rtl8852bu_fw.bin'
    if hashlib.sha256(firmware.read_bytes()).hexdigest() != SHA256:
        raise SystemExit('Bluetooth firmware SHA-256 mismatch')
    build = root / 'build/bluetooth-tests'
    build.mkdir(parents=True, exist_ok=True)
    binary = build / ('protocol-test.exe' if os.name == 'nt' else 'protocol-test')
    flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wpedantic', '-O1', '-g']
    # An absolute Xcode compiler path bypasses the /usr/bin SDK shim.
    if sys.platform == 'darwin':
        sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
        flags += ['-isysroot', sdk]
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run([compiler, *flags, str(root/'tools/bluetooth_tests/protocol_test.cpp'), '-o', str(binary)], check=True)
    env = os.environ.copy()
    if args.runtime_path:
        env['PATH'] = args.runtime_path + os.pathsep + env.get('PATH', '')
    subprocess.run([str(binary), str(firmware)], check=True, env=env)

if __name__ == '__main__':
    main()
