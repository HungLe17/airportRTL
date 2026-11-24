# Hardware and macOS compatibility

airportRTL 0.1.1 targets x86_64 macOS Tahoe **26.7.1** (build `25G241`,
Darwin `25.6.0`) with the external Skywalk and Legacy IO80211
dependencies in the [installation guide](INSTALL.md). AirLink checks
the executable identity of Legacy IO80211. Confirm the macOS build and
dependency versions before carrying the installation to a different update.
Other macOS builds and Apple silicon are outside the verified configuration.

## Wi-Fi PCI devices

All IDs in this table use PCI vendor `10ec`. Each listed ID appears in
both Wi-Fi match personalities. Successful Wi-Fi association and
over-the-air AWDL require separate hardware tests after the driver matches.

| Device | PCI device ID | Status in 0.1.1 |
| --- | --- | --- |
| RTL8852BE | `b852` | Hardware-tested on Tahoe `25G241`. Wi-Fi, AirDrop and Continuity worked on the tested system. |
| RTL8852BE | `b85b` | Matched, awaiting hardware validation. |
| RTL8852BTE | `b520` | Matched, awaiting hardware validation. |
| RTL8852CE | `c852` | Matched. Simulated 6 GHz association passed. Hardware validation is pending. |
| RTL8852AE | `8852`, `a85a` | Matched, awaiting hardware validation. These chips lack the radio channel-context operations used by the reference multichannel AWDL path. |

The driver targets one active PCIe radio. The device IDs above define the
current match list for the RTL8852 family.

## Bluetooth USB devices

Check the USB ID separately from the Wi-Fi PCI ID, even on a combo card.
The loader matches the USB device and reads its HCI identity before
choosing firmware. The current firmware profile is RTL8852BU.

| USB ID | Status in 0.1.1 |
| --- | --- |
| `13d3:3571` | Hardware-tested. Firmware handoff completed, 16 extended advertising sets were reported, and the controller acknowledged advertising commands. |
| `13d3:3570` | USB match present. Firmware and advertising depend on the controller reporting the supported HCI identity. Hardware validation is pending. |
| `0bda:b85b` | USB match present. Firmware and advertising depend on the controller reporting the supported HCI identity. Hardware validation is pending. |

With the card connected, look up the Bluetooth vendor and product IDs in
System Information → USB. Obtain the wireless controller's PCI IDs from
your hardware inventory. On Linux, `lspci -nn` and `lsusb` show the
PCI and USB IDs respectively. Include both IDs when reporting a result.

## What the hardware test covered

The verified combination is an RTL8852BE (`10ec:b852`), Bluetooth
`13d3:3571`, OpenCore 1.0.8, Tahoe 26.7.1 (`25G241`) and iPhone 8 Plus.
On that boot, the Wi-Fi link and `awdl0` were active and the tester
confirmed AirDrop and Continuity. The following details are useful
when reporting another result.

| Check | Record |
| --- | --- |
| Boot and Wi-Fi | Cold boot or restart, association, and reconnect after wake. |
| Bluetooth | Firmware handoff, BLE discovery and paired accessory reconnect. |
| AirDrop | Mac to iPhone and iPhone to Mac separately. Record whether discovery, acceptance or file delivery failed. |
| Continuity | Name the feature and direction, for example Universal Clipboard from iPhone to Mac. |

Repeat-boot and sleep/wake reliability remain open. Instant Hotspot,
Apple Watch unlock, Sidecar, Continuity Camera and iPhone Mirroring
have no result on the tested build.
