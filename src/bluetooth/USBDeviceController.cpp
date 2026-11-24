//
//  USBHCIController.cpp
//  IntelBluetoothFirmware
//
//  Created by qcwap on 2021/5/21.
//  Copyright © 2021 zxystd. All rights reserved.
//

// Adapted for the RTL8852B companion: bounded HCI-only boot upload.
#include "USBDeviceController.hpp"
#include "Log.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/clock.h>

OSDefineMetaClassAndStructors(RTW89BTTransport, OSObject)

bool RTW89BTTransport::init(IOService *client, IOUSBHostDevice *device)
{
    if (!client || !device || !OSObject::init()) return false;
    m_pClient = client; m_pDevice = device;
    m_pClient->retain(); m_pDevice->retain();
    _hciLock = IOLockAlloc();
    _abortedReadCleanup = thread_call_allocate(releaseAbortedRead, this);
    if (!_hciLock || !_abortedReadCleanup) return false;
    _deviceOpen = m_pDevice->open(m_pClient);
    if (!_deviceOpen) return false;
    mReadBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionIn, 260);
    if (!mReadBuffer) return false;
    if (mReadBuffer->prepare(kIODirectionIn) != kIOReturnSuccess) return false;
    _prepared = true;
    return true;
}

static bool isHCIInterface(const StandardUSB::InterfaceDescriptor *descriptor)
{
    // USB Bluetooth HCI interface 0, alternate 0. Interface 1 carries SCO.
    return descriptor && descriptor->bInterfaceNumber == 0 && descriptor->bAlternateSetting == 0 &&
        descriptor->bInterfaceClass == kUSBWirelessControllerInterfaceClass &&
        descriptor->bInterfaceSubClass == kUSBRFControllerSubClass &&
        descriptor->bInterfaceProtocol == kUSBBluetoothProgrammingInterfaceProtocol;
}

static bool hasHCIInterface(const StandardUSB::ConfigurationDescriptor *configuration)
{
    const StandardUSB::InterfaceDescriptor *interface = nullptr;
    while ((interface = StandardUSB::getNextInterfaceDescriptor(configuration, interface))) {
        if (isHCIInterface(interface))
            return true;
    }
    return false;
}

bool RTW89BTTransport::initConfiguration()
{
    // SET_CONFIGURATION terminates existing interface services and pipes.
    // Keep a selected configuration intact, including on a warm restart.
    if (const auto *current = m_pDevice->getConfigurationDescriptor())
        return hasHCIInterface(current);
    const auto *device = m_pDevice->getDeviceDescriptor();
    if (!device)
        return false;
    for (uint8_t i = 0; i < device->bNumConfigurations; ++i) {
        const auto *config = m_pDevice->getConfigurationDescriptor(i);
        if (!config || !config->bConfigurationValue || !hasHCIInterface(config))
            continue;
        const uint8_t value = config->bConfigurationValue;
        // Create the interface nubs before HCI setup. Publish them for native
        // matching after the loader has finished and closed its USB sessions.
        if (m_pDevice->setConfiguration(value, false) != kIOReturnSuccess)
            return false;
        _unpublishedConfiguration = value;
        return true;
    }
    return false;
}

bool RTW89BTTransport::findInterface()
{
    OSIterator *iterator = m_pDevice->getChildIterator(gIOServicePlane);
    if (!iterator) return false;
    while (auto *candidate = iterator->getNextObject()) {
        auto *interface = OSDynamicCast(IOUSBHostInterface, candidate);
        const auto *d = interface ? interface->getInterfaceDescriptor() : nullptr;
        if (!isHCIInterface(d)) continue;
        interface->retain(); m_pInterface = interface; break;
    }
    iterator->release();
    _interfaceOpen = m_pInterface && m_pInterface->open(m_pClient);
    return _interfaceOpen;
}

bool RTW89BTTransport::findPipes()
{
    const auto *config = m_pInterface->getConfigurationDescriptor();
    const auto *interface = m_pInterface->getInterfaceDescriptor();
    if (!config || !interface) return false;
    const StandardUSB::EndpointDescriptor *endpoint = nullptr;
    while ((endpoint = StandardUSB::getNextEndpointDescriptor(config, interface, endpoint))) {
        if (StandardUSB::getEndpointDirection(endpoint) == kUSBIn &&
            StandardUSB::getEndpointType(endpoint) == kUSBInterrupt) {
            m_pInterruptReadPipe = m_pInterface->copyPipe(StandardUSB::getEndpointAddress(endpoint));
            break;
        }
    }
    return m_pInterruptReadPipe != nullptr;
}

void RTW89BTTransport::interruptHandler(void *owner, void *, IOReturn status, uint32_t count)
{
    auto *self = static_cast<RTW89BTTransport *>(owner);
    IOLockLock(self->_hciLock);
    self->_status = status;
    self->_actual = count;
    self->_done = true;
    if (self->_deferredCleanup) {
        const thread_call_t cleanup = self->_abortedReadCleanup;
        IOLockUnlock(self->_hciLock);
        thread_call_enter(cleanup);
        return; // USB must return before a close can finish.
    }
    // Setup still owns the core and cannot return until it observes _done
    // under this lock. Drop the transfer reference before waking that owner.
    self->release();
    IOLockWakeup(self->_hciLock, self, false);
    IOLockUnlock(self->_hciLock);
}

void RTW89BTTransport::releaseAbortedRead(thread_call_param_t owner, thread_call_param_t)
{
    static_cast<RTW89BTTransport *>(owner)->release();
}

IOReturn RTW89BTTransport::interruptPipeRead(void *out, uint32_t capacity, uint32_t *size, uint32_t timeout)
{
    if (size) *size = 0;
    if (!out || !size || !timeout || !m_pInterruptReadPipe) return kIOReturnBadArgument;
    if (!canHandOff()) return kIOReturnNotReady;
    uint64_t deadline;
    clock_interval_to_deadline(timeout, kMillisecondScale, &deadline);
    IOLockLock(_hciLock);
    _done = false; _actual = 0; _status = kIOReturnNotReady;
    IOLockUnlock(_hciLock);
    _completion.owner = this; _completion.action = interruptHandler; _completion.parameter = nullptr;
    retain(); // also keeps the buffer, client and device alive on abort failure
    IOReturn result = m_pInterruptReadPipe->io(mReadBuffer, 260, &_completion, 0);
    if (result != kIOReturnSuccess) { release(); return result; }
    bool timedOut = false;
    IOLockLock(_hciLock);
    while (!_done) {
        if (IOLockSleepDeadline(_hciLock, this, deadline, THREAD_UNINT) != THREAD_AWAKENED && !_done) {
            timedOut = true;
            IOLockUnlock(_hciLock);
            const IOReturn aborted = m_pInterruptReadPipe->abort(IOUSBHostIOSource::kAbortAsynchronous);
            uint64_t drainDeadline;
            clock_interval_to_deadline(250, kMillisecondScale, &drainDeadline);
            IOLockLock(_hciLock);
            while (!_done) {
                if (IOLockSleepDeadline(_hciLock, this, drainDeadline, THREAD_UNINT) != THREAD_AWAKENED)
                    break;
            }
            // Readiness follows the actual completion, including a reply
            // that arrived just as cancellation was requested.
            if (!_done) {
                _deferredCleanup = true;
                IOLockUnlock(_hciLock);
                XYLog("interrupt cancellation did not drain (0x%x)\n", aborted);
                return kIOReturnTimeout;
            }
            break;
        }
    }
    result = timedOut && _status == kIOReturnAborted ? kIOReturnTimeout : _status;
    if (result == kIOReturnSuccess) {
        if (!_actual || _actual > capacity || _actual > mReadBuffer->getLength()) result = kIOReturnOverrun;
        else { memcpy(out, mReadBuffer->getBytesNoCopy(), _actual); *size = _actual; }
    }
    IOLockUnlock(_hciLock);
    return result;
}

IOReturn RTW89BTTransport::sendHCIRequest(HciCommandHdr *command, uint32_t timeout)
{
    if (!command || !_interfaceOpen || !timeout) return kIOReturnBadArgument;
    if (!canHandOff()) return kIOReturnNotReady;
    StandardUSB::DeviceRequest request = {};
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionOut, kRequestTypeClass, kRequestRecipientDevice);
    request.wLength = USBToHost16(uint16_t(HCI_COMMAND_HDR_SIZE + command->len));
    uint32_t actual = 0;
    IOReturn result = m_pInterface->deviceRequest(request, command, actual, timeout);
    return result == kIOReturnSuccess && actual != HCI_COMMAND_HDR_SIZE + command->len ? kIOReturnUnderrun : result;
}

const char *RTW89BTTransport::stringFromReturn(IOReturn code) { return m_pDevice->stringFromReturn(code); }

void RTW89BTTransport::publishInterfaces(IOUSBHostDevice &device, uint8_t configuration)
{
    const auto *current = device.getConfigurationDescriptor();
    if (!current || current->bConfigurationValue != configuration || device.isInactive())
        return;
    auto *iterator = device.getChildIterator(gIOServicePlane);
    if (!iterator) {
        XYLog("cannot enumerate USB interfaces for native matching\n");
        return;
    }
    while (auto *child = iterator->getNextObject()) {
        auto *interface = OSDynamicCast(IOUSBHostInterface, child);
        if (!interface || interface->isInactive() || interface->getProvider() != &device ||
            (interface->getState() & kIOServiceRegisteredState))
            continue;
        const auto *descriptor = interface->getConfigurationDescriptor();
        if (descriptor && descriptor->bConfigurationValue == configuration)
            interface->registerService();
    }
    iterator->release();
}

uint8_t RTW89BTTransport::releaseForHandoff()
{
    // The completion's reference keeps USB alive during a deferred abort.
    if (!canHandOff())
        return 0;
    const uint8_t configuration = _unpublishedConfiguration;
    _unpublishedConfiguration = 0;
    closeUSB();
    return configuration;
}

void RTW89BTTransport::closeUSB()
{
    if (m_pInterruptReadPipe) {
        // A submitted transfer retains this object until its callback ends;
        // free cannot run with an outstanding USB completion.
        OSSafeReleaseNULL(m_pInterruptReadPipe);
    }
    if (mReadBuffer) {
        if (_prepared) mReadBuffer->complete(kIODirectionIn);
        OSSafeReleaseNULL(mReadBuffer);
        _prepared = false;
    }
    if (m_pInterface) {
        if (_interfaceOpen) m_pInterface->close(m_pClient);
        OSSafeReleaseNULL(m_pInterface);
        _interfaceOpen = false;
    }
    if (_deviceOpen) {
        m_pDevice->close(m_pClient);
        _deviceOpen = false;
    }
}

void RTW89BTTransport::free()
{
    closeUSB();
    if (_unpublishedConfiguration)
        publishInterfaces(*m_pDevice, _unpublishedConfiguration);
    if (_abortedReadCleanup)
        thread_call_free(_abortedReadCleanup);
    if (_hciLock) { IOLockFree(_hciLock); _hciLock = nullptr; }
    OSSafeReleaseNULL(m_pClient);
    OSSafeReleaseNULL(m_pDevice);
    OSObject::free();
}
