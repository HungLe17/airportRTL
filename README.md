# airportRTL

airportRTL brings PCIe Wi-Fi for Realtek rtw89 cards to x86_64 macOS with
OpenCore. It combines the Wi-Fi radio, macOS controller and AirLink's AWDL
event adapter in one kext. A second kext loads firmware for supported USB
Bluetooth controllers and provides BLE advertising for AirDrop and Continuity.

| Kext | Purpose |
| --- | --- |
| `airportRTLWiFi.kext` | Station Wi-Fi, the AWDL interface and AirLink |
| `airportRTLBluetooth.kext` | Realtek USB firmware loading, transport handoff and BLE advertising |

Each kext has one executable and one OpenCore entry. OpenCore loads the
replacement Skywalk and Legacy IO80211 families separately.

## Quick compatibility

Version **0.1.2** matches every PCIe device ID handled by the bundled
rtw89 backend. The PCI vendor is `10ec`.

| PCIe Wi-Fi family | Models |
| --- | --- |
| RTL8851 | RTL8851BE |
| RTL8852 | RTL8852AE, RTL8852BE, RTL8852BTE (BE-VT), RTL8852CE |
| RTL8922 | RTL8922AE, RTL8922DE and their VS variants |

Version **0.1.1** booted on an RTL8852BE system with Wi-Fi PCI ID
`10ec:b852`, Bluetooth USB ID `13d3:3571`, macOS Tahoe **26.7.1**
(build `25G241`, Darwin `25.6.0`) and OpenCore **1.0.8**. Wi-Fi and
Bluetooth advertising came up, and AirDrop and Continuity worked with an
iPhone 8 Plus during that session. Other PCI IDs and repeat-boot and
sleep/wake reliability need hardware validation. Bluetooth support depends
on the USB controller and its firmware profile. [Exact PCI and USB IDs](docs/COMPATIBILITY.md)

## Install with OpenCore

Back up your working EFI and test on a spare boot drive. Download the
versioned Wi-Fi and Bluetooth assets and extract them. Each `.tar.gz`
contains a single `.kext` bundle. Copy the extracted kexts and the
external dependencies below to `EFI/OC/Kexts`.
[airportRTL releases](https://github.com/HungLe17/airportRTL/releases)

**Enable the card's Bluetooth USB port in your USB map and mark it internal.**
The Wi-Fi radio uses PCIe and the Bluetooth controller uses USB.

### Kernel → Add

Keep the following relative load order. For each entry, set `Enabled` to
`True`, use `Contents/Info.plist` for `PlistPath`, and use
`Contents/MacOS/<name>` for `ExecutablePath`, where `<name>` is the bundle
name without `.kext`.

| Order | BundlePath | Arch | MinKernel | MaxKernel | External dependency |
| --- | --- | --- | --- | --- | --- |
| 1 | `Lilu.kext` | `Any` | empty | empty | [Lilu releases](https://github.com/acidanthera/Lilu/releases) |
| 2 | `airportRTLBluetooth.kext` | `x86_64` | `25.6.0` | `25.6.0` | |
| 3 | `IOSkywalkFamily.kext` | `Any` | empty | empty | [OCLP Skywalk 1.2.0](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IOSkywalkFamily-v1.2.0.zip) |
| 4 | `IO80211FamilyLegacy.kext` | `Any` | empty | empty | [OCLP Legacy IO80211](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Wifi/IO80211FamilyLegacy-v1.0.0.zip) |
| 5 | `AMFIPass.kext` | `Any` | `20.0.0` | empty | [OCLP AMFIPass 1.4.1](https://raw.githubusercontent.com/dortania/OpenCore-Legacy-Patcher/d9604c36a432eaf243ea18659ff4d208187452d7/payloads/Kexts/Acidanthera/AMFIPass-v1.4.1-RELEASE.zip) |
| 6 | `airportRTLWiFi.kext` | `x86_64` | `25.6.0` | `25.6.0` | |
| 7 | `BlueToolFixup.kext` | `Any` | `21.0.0` | empty | [BrcmPatchRAM releases](https://github.com/acidanthera/BrcmPatchRAM/releases) |

The tested EFI used Lilu 1.7.3 and BlueToolFixup 2.7.3. Leave out
`IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext` and its
OpenCore entry. When migrating from an older driver setup, disable its
`AirPort_RTW89`, `AirPortRTW89Front`, `OwlyAirLinkBridge`, standalone
`BroadTek`, Bluetooth firmware loader and `RTW89BluetoothFixup` entries
for this card.

### Kernel → Block

Add this entry for the built-in Skywalk. Keep the external
`IOSkywalkFamily.kext` enabled in `Kernel → Add`.

| Identifier | Strategy | Arch | MinKernel | MaxKernel | Enabled |
| --- | --- | --- | --- | --- | --- |
| `com.apple.iokit.IOSkywalkFamily` | `Exclude` | `x86_64` | `23.0.0` | empty | `True` |

### Boot and verify

1. Add `-broadtekbeta` to `NVRAM → Add → 7C436110-AB2A-4BBB-A880-FE41995C9F82 → boot-args`
   for BLE advertising on Tahoe. Remove `rtw89_awdl=1` and
   `rtw89_ble_extadv=1` if they are left over from AirBridgeRTL.
2. The tested legacy Wi-Fi EFI used `Misc → Security → SecureBootModel = Disabled`.
   For the tested boot policy and settings for an EFI shared across macOS
   versions, see the [full installation guide](docs/INSTALL.md).
3. Run `ocvalidate` from the same OpenCore release, boot the test EFI, and
   check Wi-Fi, Bluetooth and AirDrop in both directions. If the test EFI
   fails to boot, select the saved working EFI from your firmware boot menu.

## Credits

- **FranciumOS and AirPort_RTW89 contributors** for the macOS rtw89 port and
  the Wi-Fi controller integration. [AirPort_RTW89](https://github.com/FranciumOS-org/AirPort_RTW89)
- **RTLAirLink and the Owly contributors** for the AWDL radio integration
  and the AirLink event bridge. [RTLAirLink](https://github.com/taingo936-ui/RTLAirLink)
- **Realtek and the Linux wireless contributors** for rtw89 and the device
  firmware. [Linux rtw89](https://github.com/torvalds/linux/tree/master/drivers/net/wireless/realtek/rtw89)
- **RealtekBluetoothFirmware contributors** for the USB Bluetooth firmware
  loader adapted by AirBridgeRTL. [Loader provenance](src/bluetooth/SOURCE.md)
- **BroadTek contributors** for the BLE advertising implementation linked
  into the Bluetooth kext. [BroadTek](https://github.com/FranciumOS-org/BroadTek)
- **Acidanthera and OpenCore Legacy Patcher contributors** for OpenCore,
  Lilu, build headers and the legacy networking dependency setup.

Source notices and firmware licenses ship with their corresponding components.
