# Hardware and macOS compatibility

airportRTL targets x86_64 macOS Tahoe **26.7.1** (build `25G241`,
Darwin `25.6.0`) with the external Skywalk and Legacy IO80211
dependencies in the [installation guide](INSTALL.md). AirLink checks
the executable identity of Legacy IO80211. Confirm the macOS build and
dependency versions before carrying the installation to a different update.
The v0.1.2 OpenCore setup pins both airportRTL kexts to Darwin `25.6.0`.
macOS Sequoia 15, Sonoma 14, Ventura 13, earlier versions and Apple silicon
are outside the release target.

## Wi-Fi PCI devices

All IDs in this table use PCI vendor `10ec`. Version 0.1.2 includes
all 12 IDs in both Wi-Fi match personalities. The match list comes from
the PCIe backends in the bundled rtw89 driver. Successful Wi-Fi association
and over-the-air AWDL require separate hardware tests.

| Device | PCI device ID | Validation |
| --- | --- | --- |
| RTL8851BE | `b851` | Matched, awaiting hardware validation. |
| RTL8852BE | `b852` | Hardware-tested on Tahoe `25G241`. Wi-Fi, AirDrop and Continuity worked on the tested system. |
| RTL8852BE | `b85b` | Matched, awaiting hardware validation. |
| RTL8852BTE (BE-VT) | `b520` | Matched, awaiting hardware validation. |
| RTL8852CE | `c852` | Matched. Simulated 6 GHz association passed. Hardware validation is pending. |
| RTL8852AE | `8852`, `a85a` | Matched, awaiting hardware validation. These chips lack the radio channel-context operations used by the reference multichannel AWDL path. |
| RTL8922AE and AE-VS | `8922`, `892b` | Matched, awaiting hardware validation. |
| RTL8922DE and DE-VS | `892d`, `882d`, `895d` | Matched, awaiting hardware validation. |

The driver targets one active PCIe radio. USB Wi-Fi adapters supported by
Linux rtw89 require a separate USB Wi-Fi transport. [Linux rtw89 configuration](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/realtek/rtw89/Kconfig)

## Bluetooth USB devices

Check the USB ID separately from the Wi-Fi PCI ID, even on a combo card.
The loader matches the USB device and reads its HCI identity before
choosing firmware. The current firmware profile is RTL8852BU. Enable the
card's Bluetooth port in your USB map and mark it internal.

| USB ID | Status |
| --- | --- |
| `13d3:3571` | Hardware-tested. Firmware handoff completed, 16 extended advertising sets were reported, and the controller acknowledged advertising commands. |
| `13d3:3570` | USB match present. Firmware and advertising depend on the controller reporting the supported HCI identity. Hardware validation is pending. |
| `0bda:b85b` | USB match present. Firmware and advertising depend on the controller reporting the supported HCI identity. Hardware validation is pending. |
| `0bda:b85c` | USB match added in 0.1.3. RTL8852BU firmware handoff was reported on Tahoe 26.7.3. macOS controller initialization and pairing still require validation. |

With the card connected, look up the Bluetooth vendor and product IDs in
System Information → USB. Obtain the wireless controller's PCI IDs from
your hardware inventory. On Linux, `lspci -nn` and `lsusb` show the
PCI and USB IDs respectively. Include both IDs when reporting a result.

## What the hardware test covered

The verified 0.1.1 combination is an RTL8852BE (`10ec:b852`), Bluetooth
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

Follow-up results for AirPods, Handoff, Universal Clipboard, iPhone Personal
Hotspot and iPhone Mirroring are in the [feature status table](../README.md#what-works-and-known-limits).
