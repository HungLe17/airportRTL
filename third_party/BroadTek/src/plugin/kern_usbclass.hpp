/* SPDX-License-Identifier: MIT */
#ifndef KERN_USBCLASS_HPP
#define KERN_USBCLASS_HPP

/* Hide USB nodes from bluetoothd's external-dongle test.
 *
 * bluetoothd will not start the LE advertising stack at all on this machine,
 * because it classifies the controller as a third-party dongle and assigns it
 * chipset 0, which fails the gate inside LE_InitMultipleAdvertising. The whole
 * classification turns on one byte -- the bDeviceClass property of an
 * IOUSBHostDevice node -- and the internal-module default it falls back to
 * instead, 3000, passes that gate. docs/LE_ADVERTISING.md has the trace.
 *
 * So this changes that one property, which is the cheapest experiment that
 * can distinguish "macOS refuses because of the classification" from "macOS
 * refuses for some further reason as well".
 *
 * It is off unless -broadtekclass is set, and it is an experiment, not a
 * feature: nothing else in BroadTek depends on it. */
void brtkUsbClassInit();

#endif
