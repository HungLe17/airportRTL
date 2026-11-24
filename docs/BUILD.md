# Building and validating airportRTL

The release build produces `airportRTL-integrated.zip`, containing the
two signed project kexts. Use the [installation guide](INSTALL.md) to
boot the resulting bundles with the external dependencies.

## Source layout

| Path | Implementation |
| --- | --- |
| [`src/bluetooth/`](../src/bluetooth/SOURCE.md) | Realtek USB attachment, HCI validation, firmware download and transport release |
| [`third_party/BroadTek/src/plugin/`](../third_party/BroadTek/src/plugin/kern_broadtek.cpp) | BroadTek's Bluetooth LE advertising implementation, linked into `airportRTLBluetooth.kext` |
| [`bluetooth/rules.mk`](../bluetooth/rules.mk) | Links the local loader and BroadTek into one module and combines their IOKit personalities |
| [`third_party/AirPort_RTW89/`](../third_party/AirPort_RTW89/CREDITS.md) | rtw89 Wi-Fi, macOS controller and AirLink's AWDL adapter |
| [`wifi/rules.mk`](../wifi/rules.mk) | Builds the Wi-Fi controller, radio and AirLink as one module |

## Build the two kexts

Build on x86_64 macOS with Xcode command-line tools, Python 3 and Make.
Use checkout and dependency paths without spaces. The target machine's
libraries are used to check kernel imports.

| Input | Version or revision |
| --- | --- |
| `FAMILY_KEXT` | The target `IO80211FamilyLegacy.kext`, UUID `4C07538B-62EB-3D6D-AEF9-93BCDCF45FAB` |
| `LILU_KEXT` | The target `Lilu.kext`. The hardware-tested setup used 1.7.3. |

Fetch the legacy family and Lilu separately and supply their paths.
The checkout includes the firmware and its checksums. Build the archive
from the repository root. [Dependency sources](INSTALL.md#1-prepare-an-efi-and-the-kexts)

```sh
make package \
  FAMILY_KEXT=/path/to/IO80211FamilyLegacy.kext \
  LILU_KEXT=/path/to/Lilu.kext
```

The default output is `build/airportRTL/airportRTL-integrated.zip`
under the repository root. The build validates firmware inputs, links
one executable per kext, checks kernel imports and verifies the ad-hoc
signatures after extracting the archive.

## Validate a build

Run the local firmware, advertising and Wi-Fi personality tests from the
repository root. The vendored AirPort_RTW89 tree provides the radio tests
and AirLink's adapter tests.

```sh
make test
make -C third_party/AirPort_RTW89 FIRMWARE_DIR="$PWD/firmware" hosttest
make -C third_party/AirPort_RTW89/bridge hosttest-sanitize bootstrap-hosttest \
  target-hosttest entitlement-hosttest native-adapter-hosttest
```

The radio host suite exercises simulated association, AWDL, 6 GHz and
teardown. For a hardware pass, check cold boot, Wi-Fi, Bluetooth, AirDrop
both ways and Continuity before and after sleep.

For reference, the executables loaded in the 0.1.1 hardware test had
the following UUIDs.

| Executable | x86_64 UUID |
| --- | --- |
| `airportRTLWiFi` | `E5014671-A8F9-3837-A423-7C6BA9B3E24D` |
| `airportRTLBluetooth` | `8DFC790B-376D-3C9B-8A18-542BBC1F9831` |

Use `dwarfdump --uuid` to identify a local executable and
`kmutil showloaded` to identify the running build. The Wi-Fi UUID can
change when rebuilding in a different checkout path. Include the tested
macOS build, both UUIDs and the hardware results with a release archive.

## Source credits

- The Wi-Fi backend and front controller come from FranciumOS
  AirPort_RTW89 at `0ad0963321cdd52e6bc77a953b3d84d25496e860`,
  which ports Linux rtw89 to macOS. AirLink is based on
  RTLAirLink's event adapter as included by FranciumOS. The airportRTL
  integration combines those pieces into one module and preserves
  their source notices. [AirPort_RTW89 revision](https://github.com/FranciumOS-org/AirPort_RTW89/tree/0ad0963321cdd52e6bc77a953b3d84d25496e860)
- The USB loader comes from AirBridgeRTL's adaptation of
  RealtekBluetoothFirmware. It validates the controller identity and
  finishes USB operations before macOS takes over. [AirBridgeRTL](https://github.com/HungLe17/AirBridgeRTL) ·
  [RealtekBluetoothFirmware source](https://github.com/thegwchr/RealtekBluetoothFirmware/tree/22e501a8a0a7aba8bb4e594bc6f40b1a67391af9)
- Bluetooth LE advertising uses BroadTek v0.2.0 at
  `2654ad368f39e833b03fd521b877a116dd015e00`, linked into the Bluetooth
  module. Its MIT notice is carried with the source and in the Bluetooth
  bundle. [BroadTek revision](https://github.com/FranciumOS-org/BroadTek/tree/2654ad368f39e833b03fd521b877a116dd015e00) ·
  [BroadTek license](../third_party/BroadTek/LICENSE)

Keep the original per-file notices and the licenses supplied with the
source and firmware. The project bundles include notices for their
packaged components. OpenCore, Lilu, BlueToolFixup, AMFIPass and the
replacement networking families are obtained separately.
