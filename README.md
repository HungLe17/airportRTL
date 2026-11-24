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
replacement Skywalk and Legacy IO80211 families separately, alongside
Lilu and the other prerequisites listed in the installation guide.

## Quick compatibility

Current source builds match every PCIe device ID handled by the bundled
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

Use `airportRTL-integrated.zip` from [Releases](https://github.com/HungLe17/airportRTL/releases)
when available, or [build it from source](docs/BUILD.md). For Tahoe 26.7.1,
use a copy of your working EFI on a spare boot drive.

1. Copy `airportRTLWiFi.kext`, `airportRTLBluetooth.kext` and the external
   dependencies into `EFI/OC/Kexts`. Leave out the Broadcom-specific
   `AirPortBrcmNIC.kext` plugin from the copied `IO80211FamilyLegacy.kext`.
   [Tested dependency versions and downloads](docs/INSTALL.md#1-prepare-an-efi-and-the-kexts)
2. **Map the card's Bluetooth USB port as internal** in your USB map.
   The Wi-Fi radio uses PCIe, and the card's Bluetooth controller must
   appear on an enabled internal USB port.
3. In `Kernel → Add`, load the kexts in this relative order.
   `Lilu` → `airportRTLBluetooth` → `IOSkywalkFamily` →
   `IO80211FamilyLegacy` → `AMFIPass` → `airportRTLWiFi` → `BlueToolFixup`.
   Set `Arch` to `x86_64` and `MinKernel` and `MaxKernel` to `25.6.0`
   on both airportRTL entries. When migrating, clear the superseded driver entries.
   [Exact OpenCore entries and migration list](docs/INSTALL.md#2-configure-kernel--add)
4. Add the `Exclude` block for the built-in
   `com.apple.iokit.IOSkywalkFamily` and keep the external replacement enabled.
   Set `-broadtekbeta` in `boot-args` for Bluetooth advertising on Tahoe.
   [Block entry, boot arguments and tested boot policy](docs/INSTALL.md#3-set-the-legacy-skywalk-block-and-boot-arguments)
5. Run the matching OpenCore `ocvalidate`, boot the test EFI and check Wi-Fi,
   Bluetooth and an AirDrop transfer in each direction. [Validation and rollback](docs/INSTALL.md#4-validate-and-boot)

[Full installation guide](docs/INSTALL.md) ·
[Troubleshooting](docs/TROUBLESHOOTING.md) ·
[Source credits and licenses](docs/BUILD.md#source-credits)
