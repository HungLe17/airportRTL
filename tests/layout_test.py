"""Both reference plists use the same personality key. Keep both PCI drivers."""
import plistlib
from pathlib import Path
import re
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "third_party/AirPort_RTW89"
sys.path.insert(0, str(ROOT / "wifi"))
from check_layout import PCI_DEVICES, merged_info


class LayoutTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.matches = " ".join(f"0x{n:04x}10ec" for n in PCI_DEVICES)
        self.write("kext", "AirPort_RTW89", "1.5.0", self.matches)
        self.write("front", "AirPortRTW89Front", "3.4", self.matches)

    def write(self, folder, name, version, matches):
        path = self.root / folder / "Info.plist"
        path.parent.mkdir(exist_ok=True)
        props = dict(IOClass=name, IOMatchCategory=name, IOProviderClass="IOPCIDevice", IOPCIMatch=matches)
        path.write_bytes(plistlib.dumps(dict(IOKitPersonalities={"RTW89": props},
                                             OSBundleLibraries={"network": version})))

    def test_both_roles_include_every_registered_rtw89_pci_device(self):
        radio_glue = (REFERENCE / "src/compat_rtw89/rtw89_glue.c").read_text()
        driver_list = radio_glue.split("#define GLUE_PCI_DRIVERS(x)", 1)[1].split("#define", 1)[0]
        registered_ids = set()
        for driver in re.findall(r"x\(rtw89_(\w+)_driver\)", driver_list):
            source = (REFERENCE / "third_party/rtw89" / f"rtw{driver}.c").read_text()
            ids = re.findall(r"PCI_DEVICE\(PCI_VENDOR_ID_REALTEK,\s*(0x[0-9a-fA-F]+)\)", source)
            self.assertTrue(ids, driver)
            registered_ids.update(f"0x{int(device, 16):04x}10ec" for device in ids)

        info = merged_info(REFERENCE / "src", "test.wifi", "0.1.0")
        roles = info["IOKitPersonalities"]
        self.assertEqual(set(roles), {"AirPort_RTW89", "AirPortRTW89Front"})
        self.assertEqual(len(registered_ids), 12)
        for name, props in roles.items():
            self.assertEqual(props["IOMatchCategory"], name)
            self.assertEqual(props["CFBundleIdentifier"], "test.wifi")
            self.assertCountEqual(props["IOPCIMatch"].split(), registered_ids)
            self.assertNotIn("0xc82210ec", props["IOPCIMatch"].split())

    def test_unmatched_card_is_not_added(self):
        self.write("front", "AirPortRTW89Front", "3.4", "0xb85210ec")
        with self.assertRaises(ValueError):
            merged_info(self.root, "test.wifi", "0.1.0")

    def test_unregistered_extra_match_is_rejected(self):
        self.write("front", "AirPortRTW89Front", "3.4", self.matches + " 0xc82210ec")
        with self.assertRaises(ValueError):
            merged_info(self.root, "test.wifi", "0.1.0")

    def test_integrated_airlink_loads_with_the_merged_wifi_bundle(self):
        role = dict(IOClass="OwlyAirLinkBridge", IOMatchCategory="OwlyAirLinkBridge",
                    IOProviderClass="IOResources", IOResourceMatch="IOKit")
        bridge = dict(IOKitPersonalities={"AirLink": role}, OSBundleRequired="Root",
                      OSBundleLibraries={"as.vit9696.Lilu": "1.7.3"})
        info = merged_info(self.root, "test.wifi", "0.1.1", bridge)
        self.assertEqual(info["OSBundleRequired"], "Root")
        self.assertEqual(info["OSBundleLibraries"]["as.vit9696.Lilu"], "1.7.3")
        self.assertEqual(info["OSBundleLibraries"]["network"], "3.4")
        self.assertEqual(info["IOKitPersonalities"]["OwlyAirLinkBridge"],
                         dict(role, CFBundleIdentifier="test.wifi"))


if __name__ == "__main__":
    unittest.main()
