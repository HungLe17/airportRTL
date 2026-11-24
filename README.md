# airportRTL

**PCIe Wi-Fi and USB Bluetooth for Realtek rtw89 cards on macOS Tahoe.**

airportRTL targets **x86_64 macOS Tahoe 26.7.1** (`25G241`, Darwin `25.6.0`)
with OpenCore and the external legacy networking stack.

| v0.1.2 download | What it provides |
| --- | --- |
| [airportRTLWiFi.kext][wifi-download] | Station Wi-Fi and the AirLink AWDL adapter |
| [airportRTLBluetooth.kext][bluetooth-download] | Realtek USB firmware loading and BLE advertising |

Each download extracts to one `.kext` bundle. [Release notes][release]

## What works and known limits

The hardware baseline is RTL8852BE (`10ec:b852`) with USB Bluetooth
`13d3:3571`. Initial tests used airportRTL v0.1.1, Tahoe 26.7.1,
OpenCore 1.0.8 and an iPhone 8 Plus. The table includes follow-up reports.

| Feature | Status | Notes |
| --- | --- | --- |
| Wi-Fi | **Works** | Joined a network on the hardware baseline. |
| Bluetooth LE advertising | **Works** | Firmware handoff and advertising verified. |
| AirPods | **Works** | Full functionality reported. |
| AirDrop | **Works** | File transfer confirmed with an iPhone 8 Plus. |
| Continuity | **Works** | General Continuity use reported. |
| Handoff | **Works** | Confirmed in follow-up testing. |
| Universal Clipboard | **Works** | Confirmed on airportRTL. |
| iPhone Personal Hotspot | **Fails** | Reported on the tested setup. |
| iPhone Mirroring | **Fails** | Reported on the tested setup. |
| Instant Hotspot | **Untested** | Automatic hotspot discovery has no separate result. |
| Continuity Camera | **Untested** | iPhone webcam mode. |
| AirPlay to Mac | **Untested** | Streaming to this Mac. |
| Apple Watch Auto Unlock | **Untested** | |
| Sidecar | **Untested** | |
| Universal Control | **Untested** | |
| Wi-Fi and Bluetooth after sleep | **Untested** | Reconnection after wake. |

The expanded v0.1.2 PCIe matches and repeat-boot reliability still need
hardware testing. The current build targets x86_64 and PCIe Wi-Fi.
Continuity features also have their own device and OS requirements.
[Apple Continuity requirements](https://support.apple.com/en-gb/108046)

## Compatibility

Version 0.1.2 matches all 12 PCIe device IDs in the bundled rtw89 driver.
All Wi-Fi IDs use vendor `10ec`.

| PCIe Wi-Fi model | Device IDs |
| --- | --- |
| RTL8851BE | `b851` |
| RTL8852AE | `8852`, `a85a` |
| RTL8852BE | `b852`, `b85b` |
| RTL8852BTE (BE-VT) | `b520` |
| RTL8852CE | `c852` |
| RTL8922AE / AE-VS | `8922`, `892b` |
| RTL8922DE / DE-VS | `892d`, `882d`, `895d` |

The Bluetooth loader targets RTL8852BU firmware. It matches USB IDs
`13d3:3571`, `13d3:3570` and `0bda:b85b` and checks the controller's HCI
identity before loading firmware. [Device and feature details](docs/COMPATIBILITY.md)

## Install with OpenCore

### 1. Prepare a test EFI

Back up your working EFI and test from a spare boot drive. Extract both
downloads and copy the `.kext` bundles and the external dependencies in
the table below to `EFI/OC/Kexts`.

> [!IMPORTANT]
> Enable the card's Bluetooth USB port in your USB map and mark it internal.
> Bluetooth needs this USB connection even when PCIe Wi-Fi is working.

### 2. Set the load order in `Kernel → Add`

Keep the following relative load order. For each entry, set `Enabled` to
`True`, use `Contents/Info.plist` for `PlistPath`, and use
`Contents/MacOS/<name>` for `ExecutablePath`, where `<name>` is the bundle
name without `.kext`.

| Order | BundlePath | Arch | MinKernel | MaxKernel | External dependency |
| --- | --- | --- | --- | --- | --- |
| 1 | `Lilu.kext` | `Any` | empty | empty | [Lilu releases][lilu] |
| 2 | `airportRTLBluetooth.kext` | `x86_64` | `25.6.0` | `25.6.0` | |
| 3 | `IOSkywalkFamily.kext` | `Any` | empty | empty | [OCLP Skywalk 1.2.0][skywalk] |
| 4 | `IO80211FamilyLegacy.kext` | `Any` | empty | empty | [OCLP Legacy IO80211][legacy-io80211] |
| 5 | `AMFIPass.kext` | `Any` | `20.0.0` | empty | [OCLP AMFIPass 1.4.1][amfipass] |
| 6 | `airportRTLWiFi.kext` | `x86_64` | `25.6.0` | `25.6.0` | |
| 7 | `BlueToolFixup.kext` | `Any` | `21.0.0` | empty | [BrcmPatchRAM releases][bluetoolfixup] |

The tested EFI used Lilu 1.7.3 and BlueToolFixup 2.7.3. Leave out
`IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext` and its
OpenCore entry. When migrating from an older driver setup, disable its
`AirPort_RTW89`, `AirPortRTW89Front`, `OwlyAirLinkBridge`, standalone
`BroadTek`, Bluetooth firmware loader and `RTW89BluetoothFixup` entries
for this card.

### 3. Add the legacy Wi-Fi block in `Kernel → Block`

Exclude the built-in Skywalk. Keep the external
`IOSkywalkFamily.kext` enabled in `Kernel → Add`.

| Use | Identifier | Strategy | Arch | MinKernel | MaxKernel | Enabled |
| --- | --- | --- | --- | --- | --- | --- |
| Required for legacy Wi-Fi | `com.apple.iokit.IOSkywalkFamily` | `Exclude` | `x86_64` | `23.0.0` | empty | `True` |
| Optional for an RTL8125 boot conflict | `com.apple.driver.AppleEthernetRL` | `Exclude` | `x86_64` | `25.0.0` | empty | `True` |

Use the optional Ethernet block if Tahoe fails to boot after enabling
replacement Skywalk and your system has the `AppleEthernetRL` conflict.
This recovered boot on the tested RTL8125 system, where wired Ethernet
continued through a Lucy driver. The working test EFI uses
`RTL812xLucy.kext`. [Ethernet driver maintainer's notes][ethernet-conflict]

### 4. Boot and verify

1. Add `-broadtekbeta` to `NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82 → boot-args`
   for BLE advertising on Tahoe. Remove `rtw89_awdl=1` and
   `rtw89_ble_extadv=1` if they are left over from AirBridgeRTL.
2. The tested legacy Wi-Fi EFI used `Misc → Security → SecureBootModel = Disabled`.
   For the tested boot policy and settings for an EFI shared across macOS
   versions, see the [full installation guide](docs/INSTALL.md).
3. Run `ocvalidate` from the same OpenCore release, boot the test EFI, and
   check Wi-Fi, Bluetooth and AirDrop in both directions. If the test EFI
   fails to boot, select the saved working EFI from your firmware boot menu.

## Build from source

Use an x86_64 Mac or Hackintosh with Xcode Command Line Tools, Python 3
and Make. Use paths without spaces and supply the target copies of
`IO80211FamilyLegacy.kext` and `Lilu.kext` from the load-order table.
Run these commands from the repository root.

```sh
make test
make package \
  FAMILY_KEXT=/path/to/IO80211FamilyLegacy.kext \
  LILU_KEXT=/path/to/Lilu.kext
```

The build writes one versioned `.kext.tar.gz` per kext to `build/airportRTL/`.

## Credits

- **FranciumOS and AirPort_RTW89 contributors** for the macOS rtw89 port and
  the Wi-Fi controller integration. [AirPort_RTW89](https://github.com/FranciumOS-org/AirPort_RTW89)
- **Anh Tài (xnpty) and RTLAirLink contributors** for the native macOS AWDL
  implementation and the AirLink event bridge. [RTLAirLink](https://github.com/xnpty/RTLAirLink)
- **Owly contributors** for AirDrop and AWDL protocol research and reference
  implementations. [Owly](https://github.com/t4t5/owly)
- **Realtek and the Linux wireless contributors** for rtw89 and the device
  firmware. [Linux rtw89](https://github.com/torvalds/linux/tree/master/drivers/net/wireless/realtek/rtw89)
- **RealtekBluetoothFirmware contributors** for the USB Bluetooth firmware
  loader adapted by AirBridgeRTL. [Loader provenance](src/bluetooth/SOURCE.md)
- **BroadTek contributors** for the BLE advertising implementation linked
  into the Bluetooth kext. [BroadTek](https://github.com/FranciumOS-org/BroadTek)
- **Acidanthera and OpenCore Legacy Patcher contributors** for OpenCore,
  Lilu, build headers and the legacy networking dependency setup.

Source notices and firmware licenses ship with their corresponding components.

[release]: https://github.com/HungLe17/airportRTL/releases/tag/v0.1.2
[wifi-download]: https://github.com/HungLe17/airportRTL/releases/download/v0.1.2/airportRTLWiFi-0.1.2.kext.tar.gz
[bluetooth-download]: https://github.com/HungLe17/airportRTL/releases/download/v0.1.2/airportRTLBluetooth-0.1.2.kext.tar.gz
[lilu]: https://github.com/acidanthera/Lilu/releases
[skywalk]: https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IOSkywalkFamily-v1.2.0.zip
[legacy-io80211]: https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IO80211FamilyLegacy-v1.0.0.zip
[amfipass]: https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Acidanthera/AMFIPass-v1.4.1-RELEASE.zip
[bluetoolfixup]: https://github.com/acidanthera/BrcmPatchRAM/releases
[ethernet-conflict]: https://github.com/Mieze/LucyRTL8125Ethernet/issues/33
