# Manual OpenCore installation

This guide covers airportRTL on x86_64 macOS Tahoe 26.7.1, build
`25G241` and Darwin `25.6.0`. The hardware test used OpenCore 1.0.8.
Check the Wi-Fi PCI ID and Bluetooth USB ID independently before installing.
[Compatibility](COMPATIBILITY.md) · [OpenCore 1.0.8](https://github.com/acidanthera/OpenCorePkg/releases/tag/1.0.8)

## 1. Prepare an EFI and the kexts

Back up your complete working EFI. Make a test copy on a spare boot drive
and confirm OpenCore can boot from it. Edit that copy first so you can
return to the original EFI from the firmware boot menu.

Download and extract the two `.kext.zip` assets for the same version
from [Releases](https://github.com/HungLe17/airportRTL/releases). If you
are building from source, follow the [build guide](BUILD.md). Copy the
extracted bundles to `EFI/OC/Kexts`.

- `airportRTLWiFi.kext`
- `airportRTLBluetooth.kext`

Use the following external components from the working setup. The linked
Skywalk and Legacy IO80211 downloads contain the same x86_64 executables
as the verified EFI.

| External component | Tested version or identity | Source |
| --- | --- | --- |
| `Lilu.kext` | 1.7.3 | [Lilu releases](https://github.com/acidanthera/Lilu/releases) |
| `IOSkywalkFamily.kext` | Download labeled 1.2.0, executable UUID `035A9AD9-6CDE-362F-8DEC-B664BC6431EC` | [Pinned OCLP payload](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IOSkywalkFamily-v1.2.0.zip) |
| `IO80211FamilyLegacy.kext` | 1200.12.2b1, executable UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB` | [Pinned OCLP payload](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IO80211FamilyLegacy-v1.0.0.zip) |
| `BlueToolFixup.kext` | 2.7.3 | [BrcmPatchRAM releases](https://github.com/acidanthera/BrcmPatchRAM/releases) |
| `AMFIPass.kext` | 1.4.1, present in the tested legacy Wi-Fi setup | [Pinned OCLP payload](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Acidanthera/AMFIPass-v1.4.1-RELEASE.zip) |

Copy the dependencies to `EFI/OC/Kexts` as separate bundles, or reuse
their matching copies from your EFI. On the test copy, leave out the
Broadcom-specific `IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext`
and its OpenCore entry. Keep the original dependency download for rollback.

The Bluetooth portion of the card is an internal USB device. Keep it
enabled in your USB map as an internal port. Use the port assigned to
your own motherboard and card.

## 2. Configure Kernel → Add

Preserve unrelated entries and their dependency order. Arrange the
entries below in the shown relative order. Set `Enabled` to `True` and
`PlistPath` to `Contents/Info.plist` on each row.

| Order | BundlePath | ExecutablePath | Arch | MinKernel | MaxKernel |
| --- | --- | --- | --- | --- | --- |
| 1 | `Lilu.kext` | `Contents/MacOS/Lilu` | `Any` | empty | empty |
| 2 | `airportRTLBluetooth.kext` | `Contents/MacOS/airportRTLBluetooth` | `x86_64` | `25.6.0` | `25.6.0` |
| 3 | `IOSkywalkFamily.kext` | `Contents/MacOS/IOSkywalkFamily` | `Any` | empty | empty |
| 4 | `IO80211FamilyLegacy.kext` | `Contents/MacOS/IO80211FamilyLegacy` | `Any` | empty | empty |
| 5 | `AMFIPass.kext` | `Contents/MacOS/AMFIPass` | `Any` | `20.0.0` | empty |
| 6 | `airportRTLWiFi.kext` | `Contents/MacOS/airportRTLWiFi` | `x86_64` | `25.6.0` | `25.6.0` |
| 7 | `BlueToolFixup.kext` | `Contents/MacOS/BlueToolFixup` | `Any` | `21.0.0` | empty |

When migrating from AirPort_RTW89 or AirBridgeRTL, disable the entries
replaced by airportRTL. Remove their copies from the test EFI if you use
an editor that automatically adds all bundles back during a snapshot.

- `AirPort_RTW89.kext` and its `AirPortRTW89Front.kext` entry
- Any standalone `OwlyAirLinkBridge.kext`
- `BroadTek.kext`
- `RTW89BluetoothFirmware.kext`, `RealtekBluetoothFirmware.kext` and
  `RTW89BluetoothFixup.kext` if you used them for this controller

## 3. Set the legacy Skywalk block and boot arguments

Under **Kernel → Block**, keep or add the required Skywalk entry below.
The block applies to the built-in Skywalk. The replacement in
`Kernel → Add` supplies Skywalk for Legacy IO80211.

| Field | Tested value |
| --- | --- |
| `Identifier` | `com.apple.iokit.IOSkywalkFamily` |
| `Strategy` | `Exclude` |
| `Arch` | `x86_64` |
| `Enabled` | `True` |
| `MinKernel` | `23.0.0` |
| `MaxKernel` | empty |

On Tahoe systems with an RTL8125 boot conflict involving `AppleEthernetRL`,
the working test EFI also uses an optional `Exclude` entry for
`com.apple.driver.AppleEthernetRL`. Set `Arch` to `x86_64`, `Enabled` to
`True` and `MinKernel` to `25.0.0`, and leave `MaxKernel` empty. On the
tested system, boot recovered and wired Ethernet continued through a
Lucy driver.

For a shared EFI that boots other macOS versions, scope the Skywalk block
and the two replacement family entries to the same applicable macOS
kernel range.

In `NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82 → boot-args`,
append `-broadtekbeta` for the integrated BroadTek v0.2.0 on Tahoe. The
working configuration also retained the reference EFI's
`-broadtekp2poff`. The integrated AWDL path starts without a separate
enable flag. When migrating from AirBridgeRTL, clear the old experimental
`rtw89_awdl=1` and `rtw89_ble_extadv=1` settings.

The working Tahoe EFI used these Bluetooth values as **Data** under
`NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82`.

| Key | Value |
| --- | --- |
| `bluetoothExternalDongleFailed` | `00` |
| `bluetoothInternalControllerInfo` | `0000000000000000000000000000` (14 bytes) |

[BlueToolFixup configuration](https://github.com/acidanthera/BrcmPatchRAM#bluetoolfixupkext)

### Boot policy on the tested EFI

The working EFI already used a legacy networking setup with reduced
boot protections. It had `Misc → Security → SecureBootModel` set to
`Disabled`, `csr-active-config` set to Data `430A0000`, and the
`amfi_get_out_of_my_way=0x1` and `amfi=0x80` boot arguments. These are
the settings carried over by the test. The minimum policy needed by
this dependency stack still needs hardware validation. Preserve your
existing working platform and USB configuration when preparing the test EFI.

## 4. Validate and boot

1. Save `config.plist` and run `ocvalidate` from the same release as the
   EFI's OpenCore binary. Check each enabled bundle and executable path.
2. Boot the test EFI and join a Wi-Fi network. Verify the Bluetooth
   controller appears and reconnect a known accessory if available.
3. With Wi-Fi and Bluetooth enabled on both devices, try sending a small
   file from the Mac to the iPhone and another from the iPhone to the Mac.
4. For Universal Clipboard, enable Handoff on both devices, sign in to
   the same Apple Account and paste a short text sample in each direction.
   [Apple's Universal Clipboard guide](https://support.apple.com/en-gb/102430)

Use the [troubleshooting guide](TROUBLESHOOTING.md) to check the loaded
bundle IDs and to distinguish discovery from a completed transfer.

## Update or roll back

For an update, replace both airportRTL bundles together in the test EFI
using the two assets from the same release. Recheck the macOS build and dependency versions
in the release's compatibility notes and validate the plist. After testing,
copy the verified configuration to your normal boot drive.

For rollback, boot the original saved EFI or restore the backed-up EFI as
a unit. The drivers, their supporting families and the Skywalk block
belong to the same configuration.
