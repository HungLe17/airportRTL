"""Package and verify the two versioned kext release assets."""
import argparse
from pathlib import Path
import plistlib
import subprocess
import tarfile
import tempfile


def verify(bundle, version):
    info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    if info["CFBundleVersion"] != version or info["CFBundleShortVersionString"] != version:
        raise ValueError(f"Unexpected version in {bundle}. Expected {version}")
    executable = bundle / "Contents/MacOS" / info["CFBundleExecutable"]
    if (executable.name != bundle.stem or list(executable.parent.iterdir()) != [executable]
            or any(bundle.rglob("*.kext"))):
        raise ValueError(f"Unexpected bundle layout in {bundle}")
    roles = info["IOKitPersonalities"].values()
    if not roles or any(role["CFBundleIdentifier"] != info["CFBundleIdentifier"] for role in roles):
        raise ValueError(f"Mismatched personality owner in {bundle}")
    if bundle.name == "airportRTLWiFi.kext":
        expected = {"AirPort_RTW89", "AirPortRTW89Front", "OwlyAirLinkBridge"}
        if {role["IOClass"] for role in roles} != expected:
            raise ValueError("Unexpected Wi-Fi personalities")
        libraries = info["OSBundleLibraries"]
        if "com.rtw89.front" in libraries:
            raise ValueError("External front dependency in Wi-Fi bundle")
        native_family = {"com.apple.iokit.IO80211Family", "com.apple.iokit.IOSkywalkFamily"}
        legacy_family = {"com.apple.iokit.IO80211FamilyLegacy"}
        declared_family = libraries.keys() & (native_family | legacy_family)
        if declared_family != legacy_family:
            raise ValueError(f"Wrong Wi-Fi family dependencies in {bundle}: "
                             f"expected {sorted(legacy_family)}, found {sorted(declared_family)}")
        if info.get("OSBundleRequired") != "Root" or "as.vit9696.Lilu" not in libraries:
            raise ValueError("Integrated AirLink requires early loading with Lilu")
    else:
        if {role["IOClass"] for role in roles} != {"RTW89BluetoothFirmware", "BroadTek"}:
            raise ValueError("Unexpected Bluetooth personalities")
        if info.get("OSBundleRequired") != "Root" or "as.vit9696.Lilu" not in info["OSBundleLibraries"]:
            raise ValueError("Bluetooth advertising requires early loading with Lilu")
    subprocess.run(["codesign", "--verify", "--deep", "--strict", bundle], check=True)


def main(root, version):
    bundles = (root / "integrated/airportRTLWiFi.kext", root / "out/airportRTLBluetooth.kext")
    with tempfile.TemporaryDirectory(dir=root) as stage:
        archives = []
        for bundle in bundles:
            verify(bundle, version)
            packed = Path(stage) / f"{bundle.stem}-{version}.kext.tar.gz"
            with tarfile.open(packed, "w:gz") as output:
                output.add(bundle, arcname=bundle.name)
            extracted = Path(stage) / bundle.stem
            extracted.mkdir()
            subprocess.run(["tar", "-xzf", packed, "-C", extracted], check=True)
            if {path.name for path in extracted.iterdir()} != {bundle.name}:
                raise ValueError(f"Unexpected archive layout in {packed}")
            verify(extracted / bundle.name, version)
            archives.append(packed)
        for packed in archives:
            destination = root / packed.name
            packed.replace(destination)
            print(destination.resolve())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("version")
    args = parser.parse_args()
    main(args.root, args.version)
