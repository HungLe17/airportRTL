#!/usr/bin/env python3
"""Check that every symbol a built kext imports is exported by a library it declares.

Usage: python3 tools/check_kpi.py build/out/AirPort_RTW89.kext

For boot-injected libraries absent from the on-disk kernel collection, add
--library-kext /path/to/verified/IO80211FamilyLegacy.kext. Compare that binary's
UUID with the loaded stack first. Export checks do not establish private ABI
layouts or successful runtime loading.
Explicit libraries must also advertise a compatible version range containing
the dependent kext's requested version; exported symbols alone are insufficient.

`kmutil libraries -p` reports, for each undefined symbol it can place, the KPI
that exports it. Anything it does not report would fail at load time with
"unresolved symbol", so compare its list against the binary's own.
"""
import argparse
import ctypes
import plistlib
import re
import subprocess
import sys
from pathlib import Path


def parse_kext_version(value):
    """Use Apple's actual vers parser, including development/beta suffixes."""
    if not isinstance(value, str) or not value or not value.isascii():
        raise ValueError(f"Invalid kext version: {value!r}")
    # OSKextVersion is a signed 64-bit integer; parse failure is -1.
    # Apple IOKitUser kext.subproj/OSKextVersion.c and OSKextLibPrivate.h.
    parser = ctypes.CDLL('/System/Library/Frameworks/IOKit.framework/IOKit').OSKextParseVersionString
    parser.argtypes = [ctypes.c_char_p]
    parser.restype = ctypes.c_int64
    result = parser(value.encode('ascii'))
    if result < 0:
        raise ValueError(f"Invalid kext version: {value!r}")
    return result


def check_library_version(info, requested):
    identifier = info.get('CFBundleIdentifier', '<unknown>')
    if 'OSBundleCompatibleVersion' not in info:
        raise ValueError(f"{identifier} lacks OSBundleCompatibleVersion; cannot serve as a library")
    low = parse_kext_version(info['OSBundleCompatibleVersion'])
    high = parse_kext_version(info.get('CFBundleVersion'))
    required = parse_kext_version(requested)
    if not low <= required <= high:
        raise ValueError(f"{identifier} requested {requested} outside compatible range "
                         f"{info['OSBundleCompatibleVersion']}..{info['CFBundleVersion']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kext", type=Path)
    parser.add_argument("--library-kext", action="append", default=[], type=Path,
                        help="Explicit target library bundle, already verified against the loaded UUID (repeatable)")
    parser.add_argument("--require-vtable-exports", action="store_true",
                        help="Require exports for reserved C++ vtable symbols too")
    args = parser.parse_args()
    kext = args.kext
    info = plistlib.loads((kext / "Contents/Info.plist").read_bytes())
    binary = kext / "Contents/MacOS" / info["CFBundleExecutable"]
    dependencies = info.get("OSBundleLibraries", {})
    declared = set(dependencies)
    explicit_libraries = []
    for library in args.library_kext:
        library_info = plistlib.loads((library / "Contents/Info.plist").read_bytes())
        identifier = library_info["CFBundleIdentifier"]
        if identifier not in declared:
            parser.error(f"Explicit library {identifier} is not declared in OSBundleLibraries")
        try:
            check_library_version(library_info, dependencies[identifier])
        except ValueError as error:
            print(f"  KPI  {error}")
            return 1
        explicit_libraries.append((library, library_info))

    nm = subprocess.run(["nm", "-u", "-j", str(binary)], capture_output=True, text=True)
    if nm.returncode:
        print(f"  KPI  nm failed (exit {nm.returncode}): {nm.stderr.strip()}")
        return 1
    wanted = {s.strip() for s in nm.stdout.splitlines() if s.strip()}

    km = subprocess.run(["kmutil", "libraries", "-p", str(kext)], capture_output=True, text=True)
    if km.returncode:
        print(f"  KPI  kmutil libraries failed (exit {km.returncode}): {(km.stdout + km.stderr).strip()}")
        return 1
    found = {}
    for m in re.finditer(r"^\s+(\S+) in \S+: (\S+) \(", km.stdout + km.stderr, re.M):
        found.setdefault(m.group(1), set()).add(m.group(2))

    # OpenCore injection does not replace the on-disk KC used by kmutil. Add
    # only real exports from explicitly supplied, declared target libraries;
    # imports absent from both sources and undeclared providers still fail.
    for library, library_info in explicit_libraries:
        identifier = library_info["CFBundleIdentifier"]
        library_binary = library / "Contents/MacOS" / library_info["CFBundleExecutable"]
        exports = subprocess.run(["nm", "-arch", "x86_64", "-g", "-U", "-j", str(library_binary)],
                                 capture_output=True, text=True)
        if exports.returncode:
            print(f"  KPI  explicit library nm failed (exit {exports.returncode}): {exports.stderr.strip()}")
            return 1
        symbols = {s.strip() for s in exports.stdout.splitlines() if s.strip()}
        if not symbols:
            print(f"  KPI  explicit library {identifier} has no x86_64 exports")
            return 1
        for symbol in symbols & wanted:
            found.setdefault(symbol, set()).add(identifier)
        print(f"  KPI  explicit target library {identifier}: {library_binary}")

    # A C++ class keeps spare vtable slots named <Class>::_RESERVED<Class><n>().
    # When a later version of the class starts using a slot, that symbol is no
    # longer exported; the kext linker fills the subclass's slot from the
    # parent's vtable instead ("pad slot" patching). Such symbols are expected
    # for IONetworkController, whose slots 2-7 are in use on current systems.
    pad_slots = set()
    if not args.require_vtable_exports:
        pad_slots = {s for s in wanted - set(found) if re.search(r"\d+_RESERVED", s)}

    missing = sorted(wanted - set(found) - pad_slots)
    undeclared = sorted(s for s in wanted & set(found) if not found[s] & declared)
    used = sorted({lib for s in wanted & set(found) for lib in found[s] & declared})

    print(f"  KPI  {len(wanted)} imports; libraries used: {', '.join(used)}")
    if pad_slots:
        print(f"    {len(pad_slots)} vtable pad slot(s) left for the kext linker to patch")
    for s in missing:
        print(f"    no KPI exports {s}")
    for s in undeclared:
        print(f"    {s} is in {', '.join(sorted(found[s]))}, not declared in OSBundleLibraries")
    return 1 if missing or undeclared else 0


if __name__ == "__main__":
    sys.exit(main())
