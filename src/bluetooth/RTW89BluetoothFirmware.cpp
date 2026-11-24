// SPDX-License-Identifier: GPL-3.0-or-later
#include "RTW89BluetoothFirmware.hpp"
#include "BtRtl.hpp"
#include "Log.h"
#include <libkern/version.h>
#include <pexpert/pexpert.h>
#include <libkern/c++/OSNumber.h>

OSDefineMetaClassAndStructors(RTW89BluetoothFirmware, IOService)

// Read without allocation/registry locks from the optional Lilu page hook.
// False until real post-firmware HCI replies were checked and USB released.
static bool advertisingProof;
extern "C" bool rtw89_bt_extadv_ready(void)
{
    return __atomic_load_n(&advertisingProof, __ATOMIC_ACQUIRE);
}

IOService *RTW89BluetoothFirmware::probe(IOService *provider, SInt32 *score)
{
    int enabled = 1;
    PE_parse_boot_argn("rtw89_bt", &enabled, sizeof(enabled));
    // Initial target is Tahoe (Darwin 25), with an explicit disable switch.
    if (!enabled || version_major != 25 || !OSDynamicCast(IOUSBHostDevice, provider))
        return nullptr;
    return IOService::probe(provider, score);
}

bool RTW89BluetoothFirmware::start(IOService *provider)
{
    __atomic_store_n(&advertisingProof, false, __ATOMIC_RELEASE);
    if (!IOService::start(provider)) return false;
    auto *device = OSDynamicCast(IOUSBHostDevice, provider);
    setProperty("RTW89BTImplementation", "experimental-usb-firmware-loader");
    setProperty("RTW89BTHardwareVerified", false);
    if (!device)
        return false;
    // All USB work finishes synchronously in start(). The transport drains
    // asynchronous interrupt completions before freeing their buffers/context.
    auto *core = new RTW89BTFirmwareCore;
    const bool ok = core && core->initWithDevice(this, device) && core->setup();
    bool proved = false;
    if (ok) {
        char name[48] = {};
        core->getFirmwareName(name, sizeof(name));
        setProperty("fw_name", name);
        const auto &caps = core->advertisingCapabilities();
        setProperty("RTW89BTCapabilityReadMask", caps.readMask, 32);
        setProperty("RTW89BTLEExtendedAdvertising", caps.extended);
        setProperty("RTW89BTAdvertisingCommands", caps.commands);
        setProperty("RTW89BTMaxAdvertisingDataLength", caps.maxDataLength, 32);
        setProperty("RTW89BTAdvertisingSets", caps.sets, 32);
        auto *vendor = OSDynamicCast(OSNumber, device->getProperty("idVendor"));
        auto *product = OSDynamicCast(OSNumber, device->getProperty("idProduct"));
        // This policy was audited for the user's 13d3:3571, not every USB
        // identity whose firmware this loader can upload.
        proved = vendor && product && vendor->unsigned32BitValue() == 0x13d3 &&
            product->unsigned32BitValue() == 0x3571 && caps.ready();
    }
    // A safe handoff closes the interface and device before publication.
    // After an undrained failure, a worker closes both once USB completes.
    const uint8_t configuration = core ? core->releaseUSBForHandoff() : 0;
    OSSafeReleaseNULL(core);
    __atomic_store_n(&advertisingProof, proved, __ATOMIC_RELEASE);
    setProperty("RTW89BTExtAdvProof", proved);
    if (ok) {
        // Publish readiness before exposing the native transport handoff:
        // matching must not race the last USB close/proof publication.
        setProperty("FirmwareLoaded", true);
        device->setProperty("FirmwareLoaded", true);
        device->setName("Bluetooth USB Host Controller");
    }
    setProperty("RTW89BTStatus", ok ? "firmware-loaded-transport-released" : "setup-failed");
    device->setProperty("RTW89BTStatus", ok ? "firmware-loaded-transport-released" : "setup-failed");
    if (configuration)
        RTW89BTTransport::publishInterfaces(*device, configuration);
    XYLog("setup %s; native Bluetooth and BLE still require runtime validation\n", ok ? "complete" : "failed");
    return ok;
}
