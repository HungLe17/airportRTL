"""Both reference plists use the same personality key. Keep both PCI drivers."""
import plistlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "wifi"))
from check_layout import PCI_DEVICES, merged_info


class LayoutTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.matches = " ".join(f"0x{n:04x}10ec" for n in (*PCI_DEVICES, 0x8922))
        self.write("kext", "AirPort_RTW89", "1.5.0", self.matches)
        self.write("front", "AirPortRTW89Front", "3.4", self.matches)

    def write(self, folder, name, version, matches):
        path = self.root / folder / "Info.plist"
        path.parent.mkdir(exist_ok=True)
        props = dict(IOClass=name, IOMatchCategory=name, IOProviderClass="IOPCIDevice", IOPCIMatch=matches)
        path.write_bytes(plistlib.dumps(dict(IOKitPersonalities={"RTW89": props},
                                             OSBundleLibraries={"network": version})))

    def test_both_roles_match_the_six_ids(self):
        info = merged_info(self.root, "test.wifi", "0.1.0")
        roles = info["IOKitPersonalities"]
        self.assertEqual(set(roles), {"AirPort_RTW89", "AirPortRTW89Front"})
        for name, props in roles.items():
            self.assertEqual(props["IOMatchCategory"], name)
            self.assertEqual(props["CFBundleIdentifier"], "test.wifi")
            self.assertEqual(len(props["IOPCIMatch"].split()), 6)
            self.assertNotIn("8922", props["IOPCIMatch"])
        self.assertEqual(info["OSBundleLibraries"]["network"], "3.4")

    def test_unmatched_card_is_not_added(self):
        self.write("front", "AirPortRTW89Front", "3.4", "0xb85210ec")
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
        self.assertEqual(info["IOKitPersonalities"]["OwlyAirLinkBridge"],
                         dict(role, CFBundleIdentifier="test.wifi"))


if __name__ == "__main__":
    unittest.main()
