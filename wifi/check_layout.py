"""Check the proposed two-personality Wi-Fi layout against the target libraries."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile


# PCI vendor 10EC. Firmware is selected by the radio driver after PCI match.
PCI_DEVICES = (0xB852, 0xB85B, 0xC852, 0xB520, 0x8852, 0xA85A)


def merged_info(reference, identifier, version, bridge=None):
    radio, front = [plistlib.loads((reference / name / "Info.plist").read_bytes())
                    for name in ("kext", "front")]
    devices = [f"0x{device:04x}10ec" for device in PCI_DEVICES]
    personalities = {}
    for source, service in ((radio, "AirPort_RTW89"), (front, "AirPortRTW89Front")):
        personality, = source["IOKitPersonalities"].values()
        if (personality["IOClass"] != service or personality["IOMatchCategory"] != service
                or personality["IOProviderClass"] != "IOPCIDevice"
                or not set(devices).issubset(personality["IOPCIMatch"].lower().split())):
            raise ValueError(f"unexpected PCI matching contract for {service}")
        personalities[service] = dict(personality, CFBundleIdentifier=identifier,
                                      IOPCIMatch=" ".join(devices))
    sources = [radio, front]
    if bridge is not None:
        service = "OwlyAirLinkBridge"
        personality, = bridge["IOKitPersonalities"].values()
        if (personality["IOClass"] != service or personality["IOMatchCategory"] != service
                or personality["IOProviderClass"] != "IOResources"
                or personality.get("IOResourceMatch") != "IOKit"
                or bridge.get("OSBundleRequired") != "Root"):
            raise ValueError("unexpected AirLink service matching contract")
        personalities[service] = dict(personality, CFBundleIdentifier=identifier)
        sources.append(bridge)
        radio["OSBundleRequired"] = bridge["OSBundleRequired"]
    libraries = {}
    for source in sources:
        for name, requirement in source["OSBundleLibraries"].items():
            required = tuple(map(int, requirement.split(".")))
            if name not in libraries or required > tuple(map(int, libraries[name].split("."))):
                libraries[name] = requirement
    return dict(radio, CFBundleName="airportRTLWiFi", CFBundleDisplayName="airportRTL Wi-Fi",
                CFBundleExecutable="airportRTLWiFi",
                CFBundleIdentifier=identifier, CFBundleVersion=version, CFBundleShortVersionString=version,
                IOKitPersonalities=personalities, OSBundleLibraries=libraries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("family", type=Path)
    parser.add_argument("identifier")
    parser.add_argument("version")
    parser.add_argument("--bridge", type=Path, help="plist for the linked AirLink service")
    parser.add_argument("--lilu", type=Path, help="target Lilu kext for AirLink imports")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if bool(args.bridge) != bool(args.lilu):
        parser.error("the integrated bridge requires both --bridge and --lilu")
    bridge = plistlib.loads(args.bridge.read_bytes()) if args.bridge else None
    info = merged_info(args.reference, args.identifier, args.version, bridge)
    plist = plistlib.dumps(info, sort_keys=False)
    checker = Path(__file__).resolve().parents[1] / "tools" / "check_kpi.py"
    with tempfile.TemporaryDirectory(dir=args.binary.parent) as staging:
        bundle = Path(staging) / "airportRTLWiFi.kext"
        contents = bundle / "Contents"
        (contents / "MacOS").mkdir(parents=True)
        (contents / "Info.plist").write_bytes(plist)
        shutil.copy2(args.binary, contents / "MacOS" / info["CFBundleExecutable"])
        libraries = ["--library-kext", args.family]
        if args.lilu:
            libraries.extend(["--library-kext", args.lilu, "--require-vtable-exports"])
        subprocess.run([sys.executable, checker, bundle, *libraries], check=True)
        if args.output:
            resources = contents / "Resources"
            resources.mkdir()
            root = args.reference.parent
            for source, name in (("LICENSE", "LICENSE"), ("CREDITS.md", "CREDITS.md"),
                                 ("CREDITS-RTLAirLink.md", "CREDITS-RTLAirLink.md"),
                                 ("MODIFICATIONS-RTLAirLink.md", "MODIFICATIONS-RTLAirLink.md"),
                                 ("firmware/LICENCE.rtlwifi_firmware.txt", "Firmware-License.txt"),
                                 ("third_party/hostap/COPYING", "hostap-COPYING"),
                                 ("third_party/mbedtls/LICENSE", "mbedtls-LICENSE")):
                shutil.copy2(root / source, resources / name)
            if args.bridge:
                shutil.copy2(args.bridge.parent / "vendor/Lilu/LICENSE.txt", resources / "LICENSE-Lilu.txt")
            subprocess.run(["codesign", "--force", "--sign", "-", bundle], check=True)
            subprocess.run(["codesign", "--verify", "--deep", "--strict", bundle], check=True)
            args.output.mkdir(parents=True, exist_ok=True)
            destination = args.output / bundle.name
            if destination.exists():
                destination.rename(Path(staging) / "previous")
            try:
                bundle.rename(destination)
            except OSError:
                previous = Path(staging) / "previous"
                if previous.exists():
                    previous.rename(destination)
                raise
    (args.binary.parent / "airportRTLWiFi.plist").write_bytes(plist)


if __name__ == "__main__":
    main()
