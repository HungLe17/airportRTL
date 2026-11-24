//
//  USBHCIController.hpp
//  IntelBluetoothFirmware
//
//  Created by qcwap on 2021/5/21.
//  Copyright © 2021 zxystd. All rights reserved.
//

#ifndef USBDeviceController_hpp
#define USBDeviceController_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/usb/USB.h>
#include <libkern/OSKextLib.h>
#include <kern/thread_call.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>

#include "Hci.h"

class RTW89BTTransport : public OSObject {
    OSDeclareDefaultStructors(RTW89BTTransport)

public:

    virtual bool init(IOService *client, IOUSBHostDevice *dev);

    virtual void free() override;

    virtual bool initConfiguration();

    virtual bool findInterface();

    virtual bool findPipes();

    IOReturn interruptPipeRead(void *buf, uint32_t buf_size, uint32_t *size, uint32_t timeout);

    IOReturn sendHCIRequest(HciCommandHdr *cmd, uint32_t timeout);

    // Only the serialized setup thread reads/writes this state.
    bool canHandOff() const { return !_deferredCleanup; }
    // Close USB and return the cold configuration that the caller must publish
    // after setting firmware readiness. Zero means there is nothing to publish.
    uint8_t releaseForHandoff();
    static void publishInterfaces(IOUSBHostDevice &, uint8_t configuration);

    const char* stringFromReturn(IOReturn code);

    static void interruptHandler(void *owner, void *parameter, IOReturn status, uint32_t bytesTransferred);

private:
    void closeUSB();
    IOUSBHostDevice* m_pDevice = nullptr; // retained through USB completion
    IOService* m_pClient = nullptr;
    IOUSBHostInterface* m_pInterface = nullptr; // retained until all I/O drains

    IOUSBHostPipe* m_pInterruptReadPipe = nullptr;

    // Setup is serialized by IOService::start. Only the completion takes this
    // lock concurrently. Never call USB methods with the lock held.
    IOLock *_hciLock = nullptr;
    IOBufferMemoryDescriptor* mReadBuffer = nullptr;
    bool _prepared = false, _done = false;
    bool _deviceOpen = false, _interfaceOpen = false;
    uint8_t _unpublishedConfiguration = 0;
    // If cancellation has not completed by its deadline, the USB callback
    // schedules release of the transfer from outside the USB workloop.
    static void releaseAbortedRead(thread_call_param_t, thread_call_param_t);
    thread_call_t _abortedReadCleanup = nullptr;
    bool _deferredCleanup = false;
    IOReturn _status = kIOReturnNotReady;
    uint32_t _actual = 0;
    IOUSBHostCompletion _completion = {};
};

#endif /* USBDeviceController_hpp */
