/* SPDX-License-Identifier: MIT */
#include "kern_config.hpp"
#include "kern_broadtek.hpp"

#include <Headers/kern_util.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSBoolean.h>

OSDefineMetaClassAndStructors(BroadTekConfig, IOService);

/* address type (1) + identity address (6) + IRK (16), all in HCI byte order,
 * least significant byte first. */
static constexpr size_t kInstallBlobLen = 1 + 6 + 16;

static const char *kInstallKey = "broadtek-install-irk";
static const char *kPromoteKey = "broadtek-promote-captures";
static const char *kMapKey = "broadtek-install-map";
static constexpr size_t kMapBlobLen = 1 + 6 + 6;

bool BroadTekConfig::start(IOService *provider)
{
    if (!IOService::start(provider)) return false;
    /* Published so a helper can find it by class name. */
    registerService();
    SYSLOG("broadtek", "config service ready");
    return true;
}

IOReturn BroadTekConfig::setProperties(OSObject *properties)
{
    /* A key installed by anyone would let any process decide which device the
     * controller resolves and connects to. Root only. */
    if (IOUserClient::clientHasPrivilege(current_task(),
                                        kIOClientPrivilegeAdministrator) != kIOReturnSuccess) {
        SYSLOG("broadtek", "refusing a key install from an unprivileged caller");
        return kIOReturnNotPrivileged;
    }

    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    if (dict == nullptr) return kIOReturnBadArgument;

    IOReturn result = kIOReturnUnsupported;

    if (OSData *blob = OSDynamicCast(OSData, dict->getObject(kInstallKey))) {
        if (blob->getLength() != kInstallBlobLen) {
            SYSLOG("broadtek", "key install: wrong length %u, want %zu",
                   blob->getLength(), kInstallBlobLen);
            return kIOReturnBadArgument;
        }
        const uint8_t *b = static_cast<const uint8_t *>(blob->getBytesNoCopy());
        if (b == nullptr) return kIOReturnBadArgument;

        /* Nothing about the key is logged, here or anywhere: only the
         * identity it belongs to, which is not a secret. */
        bool ok = brtkInstallKey(b, kInstallBlobLen);
        SYSLOG("broadtek", "key install for %02x:%02x:%02x:%02x:%02x:%02x: %s",
               b[6], b[5], b[4], b[3], b[2], b[1], ok ? "stored" : "refused");
        result = ok ? kIOReturnSuccess : kIOReturnNoSpace;
    }

    if (OSData *blob = OSDynamicCast(OSData, dict->getObject(kMapKey))) {
        if (blob->getLength() != kMapBlobLen) {
            SYSLOG("broadtek", "map install: wrong length %u, want %zu",
                   blob->getLength(), kMapBlobLen);
            return kIOReturnBadArgument;
        }
        const uint8_t *b = static_cast<const uint8_t *>(blob->getBytesNoCopy());
        if (b == nullptr) return kIOReturnBadArgument;

        /* Both addresses are broadcast in the clear, so both can be logged:
         * which identity was mapped to which private address is exactly what
         * has to be checkable when a connection still does not form. */
        bool ok = brtkInstallMap(b, kMapBlobLen);
        SYSLOG("broadtek", "map %02x:%02x:%02x:%02x:%02x:%02x -> "
               "%02x:%02x:%02x:%02x:%02x:%02x: %s",
               b[6], b[5], b[4], b[3], b[2], b[1],
               b[12], b[11], b[10], b[9], b[8], b[7], ok ? "stored" : "refused");
        result = ok ? kIOReturnSuccess : kIOReturnNoSpace;
    }

    if (OSBoolean *promote = OSDynamicCast(OSBoolean, dict->getObject(kPromoteKey))) {
        if (promote->isTrue()) {
            int n = brtkPromoteCaptures();
            SYSLOG("broadtek", "promoted %d capture(s) to the resolving list", n);
            result = kIOReturnSuccess;
        }
    }

    return result;
}
