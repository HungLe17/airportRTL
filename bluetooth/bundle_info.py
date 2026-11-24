"""Combine the USB firmware loader and BroadTek startup match in one bundle."""
import plistlib
import sys
from pathlib import Path


def main():
    source, advertising, destination, executable, identifier, version = sys.argv[1:]
    info = plistlib.loads(Path(source).read_bytes())
    advertiser = plistlib.loads(Path(advertising).read_bytes())
    old_identifier = info["CFBundleIdentifier"]
    personalities = info["IOKitPersonalities"]
    for personality in personalities.values():
        if personality["CFBundleIdentifier"] != old_identifier:
            raise ValueError("unexpected external USB personality")
        personality["CFBundleIdentifier"] = identifier
    startup = advertiser["IOKitPersonalities"]["$(PRODUCT_NAME)"]
    personalities["BroadTek"] = dict(startup, CFBundleIdentifier=identifier,
                                     IOClass="BroadTek", IOMatchCategory="BroadTek")
    libraries = info["OSBundleLibraries"]
    for library, requirement in advertiser["OSBundleLibraries"].items():
        if tuple(map(int, requirement.split("."))) > tuple(map(int, libraries.get(library, "0").split("."))):
            libraries[library] = requirement
    info.update(CFBundleIdentifier=identifier, CFBundleExecutable=executable,
                CFBundleName=executable, CFBundleDisplayName="airportRTL Bluetooth", CFBundleVersion=version,
                CFBundleShortVersionString=version, OSBundleCompatibleVersion=version,
                OSBundleRequired=advertiser["OSBundleRequired"])
    Path(destination).write_bytes(plistlib.dumps(info, sort_keys=False))


if __name__ == "__main__":
    main()
