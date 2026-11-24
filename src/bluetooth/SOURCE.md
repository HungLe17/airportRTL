# Bluetooth companion source

The USB firmware loader, firmware parser and headers are adapted from
[thegwchr/RealtekBluetoothFirmware](https://github.com/thegwchr/RealtekBluetoothFirmware/tree/22e501a8a0a7aba8bb4e594bc6f40b1a67391af9),
revision `22e501a8a0a7aba8bb4e594bc6f40b1a67391af9`, retrieved 2026-10-26.
Original per-file notices and the upstream COPYING are retained. Headers with
GPL-3.0-only notices remain under those terms; this companion is a separate
GPL-3.0 kext and does not link into the Wi-Fi binary.

Local changes: narrow controller/USB/OS matching; unique class/bundle names;
bounded, hash-verified uncompressed firmware; reject partial firmware parses;
strict HCI event validation; real download acknowledgements; interrupt read
predicate locking, completion-status propagation and bounded cancellation.
If a read remains pending after cancellation, the transport retains its
USB resources until completion. The loader omits the unused bulk and
power-management paths. The tested `13d3:3571` controller completed
USB firmware handoff. [Hardware test](../../docs/COMPATIBILITY.md#what-the-hardware-test-covered)

The standard Realtek sequence and RTL8852B HCI tuple were cross-checked against
[Linux btrtl.c](https://code.googlesource.com/linux/torvalds/linux/+/23eb7901811a06805ec7d6e766dd82f083d03204/drivers/bluetooth/btrtl.c)
at `23eb7901811a06805ec7d6e766dd82f083d03204`. The local `13D3:3571`
personality also occurs in the pinned loader and in
[Linux's USB-ID addition](https://git.zx2c4.com/linux-rng/commit/drivers/bluetooth?id=069f534247bb6db4f8c2c2ea8e9155abf495c37e).

`firmware/rtl_bt/rtl8852bu_fw.bin` is unchanged from that pinned loader's
`fw/rtl8852bu_fw.bin`, SHA-256
`c2b0d34a4ff8a8ea136993f0dc4ba657522d84231b4094c2a53cda39afc91c57`.
The Bluetooth kext embeds this file with its accompanying Realtek notice.
The USB loader completes firmware handoff. BroadTek supplies the separate
advertising implementation in the same module. [BroadTek source](../../third_party/BroadTek/src/plugin/kern_broadtek.cpp)
