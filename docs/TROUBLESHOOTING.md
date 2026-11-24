# Troubleshooting airportRTL

Start by identifying the failing stage. Station Wi-Fi can work while
Bluetooth discovery or the AWDL data path is unavailable.

## Check the running configuration

```sh
sw_vers -buildVersion
uname -r
kmutil showloaded | grep -E 'airportRTL(WiFi|Bluetooth)|IO80211FamilyLegacy|IOSkywalkFamily|Lilu|BlueToolFixup|AMFIPass'
```

The project entries should be `io.github.HungLe17.airportRTLWiFi` and
`io.github.HungLe17.airportRTLBluetooth`. Check the two networking
family UUIDs against the [installation guide](INSTALL.md#1-prepare-an-efi-and-the-kexts).
The built-in and replacement Skywalk bundles share the same identifier.
Their executable UUIDs distinguish them.

## Wi-Fi is missing

Check the [PCI match table](COMPATIBILITY.md#wi-fi-pci-devices), the
`Kernel → Add` order, the Skywalk block, and the loaded Skywalk and Legacy
IO80211 UUIDs. Confirm the test EFI actually booted by checking the
airportRTL bundle identifiers. Restore the earlier complete EFI if the Mac
stops booting. [Manual OpenCore installation](INSTALL.md)

## The iPhone cannot see the Mac

Check that the internal USB Bluetooth port is mapped and that
`airportRTLBluetooth.kext` loads after Lilu. Check that the effective
boot arguments include `-broadtekbeta`. The old standalone advertising
and firmware entries are replaced by the integrated Bluetooth bundle.

```sh
ioreg -r -c RTW89BluetoothFirmware -d 1 | grep -E 'CFBundleIdentifierKernel|FirmwareLoaded|RTW89BTStatus|RTW89BTAdvertisingSets'
ioreg -lw0 | grep -E '"broadtek-(started|btd-pages-patched|adv-injected|adv-inj-replies|adv-inj-failed|adv-inj-timeouts)"'
```

Look for `FirmwareLoaded = Yes` and
`RTW89BTStatus = "firmware-loaded-transport-released"`. On the tested
controller, the loader reported 16 advertising sets, BroadTek reported
startup, and advertising address commands received matching replies.
Try discovery with AirDrop open on both devices and with AirDrop reception
enabled on the receiving device. [Apple's AirDrop guide](https://support.apple.com/en-by/guide/mac-help/mh35868/mac)

## Devices appear but AirDrop stays on Waiting

Open AirDrop, start a test transfer and check the live AWDL and AirLink
state.

```sh
ifconfig awdl0
ioreg -r -c OwlyAirLinkBridge -d 1 | grep -E 'CFBundleIdentifierKernel|OwlyEB1(BootstrapAccepted|FamilyVerified|FrontendVerified|HooksReady|NativeClientBound|ActionEventCalls)'
```

On the working boot, the AirLink service belonged to
`io.github.HungLe17.airportRTLWiFi`, the family and front checks passed,
and `OwlyEB1NativeClientBound` was `Yes`. The action event counter
increased as traffic arrived. Record the outcome for each transfer direction
and whether it reached acceptance or delivery.

If Universal Clipboard is the failing feature, verify Wi-Fi, Bluetooth and
Handoff on both devices and confirm their Apple Accounts match. Try plain
text before testing other content. [Apple's Universal Clipboard guide](https://support.apple.com/en-gb/102430)

## Report a result or recover from a failed boot

Boot the saved working EFI first if the Mac hangs or panics. If a panic
report was saved, extract the panic reason and driver backtrace from that
attempt. For a report, include the airportRTL version, the loaded kext
UUIDs, the macOS build, the PCI and USB IDs, and the feature or direction
that failed. Mention whether it happened on a cold boot, after wake, or
only during transfer. Share selected diagnostics with account details,
serial numbers and network names redacted.
