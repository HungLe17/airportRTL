# airportRTL

airportRTL brings PCIe Wi-Fi and USB Bluetooth to Realtek RTL8852 combo cards
on x86_64 macOS with OpenCore. It combines the Wi-Fi radio, the macOS front
controller and AirLink's AWDL event adapter in one kext. A second kext
loads Bluetooth firmware and provides the Bluetooth LE advertising used for
discovery by AirDrop and Continuity.

| Kext | Purpose |
| --- | --- |
| `airportRTLWiFi.kext` | Station Wi-Fi, the AWDL interface and AirLink |
| `airportRTLBluetooth.kext` | Realtek USB firmware loading, transport handoff and BroadTek advertising |

Each kext has one executable and one OpenCore entry. OpenCore loads the
replacement Skywalk and Legacy IO80211 families separately, alongside
Lilu and the other prerequisites listed in the installation guide.

## Hardware-tested configuration

Version **0.1.1** booted on an RTL8852BE system with Wi-Fi PCI ID
`10ec:b852`, Bluetooth USB ID `13d3:3571`, macOS Tahoe **26.7.1**
(build `25G241`, Darwin `25.6.0`) and OpenCore **1.0.8**. Wi-Fi and
Bluetooth advertising came up, and AirDrop and Continuity worked with an
iPhone 8 Plus during that session. Repeat boots, sleep/wake and other
RTL8852 variants still need testing.

## Get started

Use `airportRTL-integrated.zip` from [Releases](https://github.com/HungLe17/airportRTL/releases)
when available, or build it from this checkout. The archive contains the two
airportRTL kexts. The installation guide gives the OpenCore load order and
links to the external dependencies.

- [Install, update and roll back](docs/INSTALL.md)
- [Supported and experimental devices](docs/COMPATIBILITY.md)
- [Troubleshooting and diagnostics](docs/TROUBLESHOOTING.md)
- [Build inputs, validation and credits](docs/BUILD.md)

airportRTL builds on FranciumOS's AirPort_RTW89 and BroadTek, RTLAirLink's
AirLink adapter, the Linux rtw89 driver and RealtekBluetoothFirmware.
Original source notices and firmware licenses accompany the corresponding
components. [Source credits](docs/BUILD.md#source-credits)
