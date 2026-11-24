// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBHostDevice.h>

// This independent USB kext uploads firmware then releases the interfaces to
// Apple's Bluetooth transport. It does not implement HCI privacy/advertising
// policy, patch bluetoothd, or claim successful BLE/Continuity operation.
class RTW89BluetoothFirmware : public IOService {
    OSDeclareDefaultStructors(RTW89BluetoothFirmware)
public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
};
