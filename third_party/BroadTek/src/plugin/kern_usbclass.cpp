/* SPDX-License-Identifier: MIT */
#include "kern_usbclass.hpp"

#include <Headers/kern_util.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IORegistryEntry.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <pexpert/pexpert.h>
#include <string.h>

/* The classes bluetoothd accepts, as the immediate 0x80008001 tested against
 * (bDeviceClass - 0xe0) at 0x1000e9bb9: bits 0, 15 and 31, so 0xE0, 0xEF and
 * 0xFF -- Wireless Controller, Miscellaneous and Vendor Specific. */
static bool classIsTaken(uint32_t cls)
{
    uint32_t i = cls - 0xe0;
    if (i > 0x1f) return false;
    return ((0x80008001u >> i) & 1u) != 0;
}

/* What to put there instead. 0x00 means "see the interface descriptors",
 * which is the ordinary value for a composite device and is outside the set
 * above. Override with broadtekclass=<n> if something objects to 0. */
static uint32_t g_new_class = 0x00;

/* Off unless asked for; and a separate switch for looking without touching,
 * because the first thing worth knowing is which nodes would be affected. */
static bool g_enabled;
static bool g_dry_run;

static int g_changed;
static char g_report[256];

static void publish(const char *key, const char *value)
{
    OSString *s = OSString::withCString(value);
    IORegistryEntry *platform = IOService::getPlatform();
    if (s != nullptr) {
        if (platform != nullptr) platform->setProperty(key, s);
        s->release();
    }
}

static uint32_t numberProperty(IOService *svc, const char *key, bool *found)
{
    OSNumber *n = OSDynamicCast(OSNumber, svc->getProperty(key));
    if (n == nullptr) {
        if (found != nullptr) *found = false;
        return 0;
    }
    if (found != nullptr) *found = true;
    return static_cast<uint32_t>(n->unsigned32BitValue());
}

/* Runs on gIOMatchedNotification, not gIOFirstPublishNotification: by then the
 * node's own drivers have matched, so changing a property they matched on
 * cannot change which driver claimed it. bluetoothd's probe is a userspace
 * read that happens much later, so being this late costs nothing and avoids
 * the one way this experiment could take the webcam or the Bluetooth radio
 * away entirely. */
static bool onUsbDevice(void *, void *, IOService *svc, IONotifier *)
{
    if (svc == nullptr) return true;

    bool haveVendor = false, haveProduct = false, haveClass = false;
    uint32_t vendor  = numberProperty(svc, "idVendor", &haveVendor);
    uint32_t product = numberProperty(svc, "idProduct", &haveProduct);
    uint32_t cls     = numberProperty(svc, "bDeviceClass", &haveClass);

    /* A node missing any of those is already skipped by bluetoothd, so there
     * is nothing to hide. */
    if (!haveVendor || !haveProduct || !haveClass) return true;
    if (!classIsTaken(cls)) return true;

    const char *name = "?";
    if (OSString *s = OSDynamicCast(OSString, svc->getProperty("USB Product Name")))
        name = s->getCStringNoCopy();

    if (g_dry_run) {
        SYSLOG("broadtek", "usbclass: %04x:%04x class 0x%02x (\"%s\") would be hidden",
               vendor, product, cls, name);
        return true;
    }

    OSNumber *replacement = OSNumber::withNumber(g_new_class, 32);
    if (replacement == nullptr) return true;
    bool ok = svc->setProperty("bDeviceClass", replacement);
    replacement->release();

    SYSLOG("broadtek", "usbclass: %04x:%04x (\"%s\") class 0x%02x -> 0x%02x: %s",
           vendor, product, name, cls, g_new_class, ok ? "done" : "refused");

    if (!ok) return true;

    /* One line in the registry naming everything that was changed, so a
     * machine that behaves oddly afterwards is not a mystery. The property is
     * also how the experiment is read back without needing the log, which on
     * this machine wraps within minutes of boot. */
    size_t at = strlen(g_report);
    if (at < sizeof(g_report) - 1)
        snprintf(g_report + at, sizeof(g_report) - at,
             "%s%04x:%04x 0x%02x->0x%02x", at != 0 ? ", " : "",
             vendor, product, cls, g_new_class);
    publish("broadtek-usbclass", g_report);
    g_changed++;
    return true;
}

void brtkUsbClassInit()
{
    g_dry_run = checkKernelArgument("-broadtekclassdry");
    g_enabled = g_dry_run || checkKernelArgument("-broadtekclass");
    if (!g_enabled) return;

    uint32_t wanted = 0;
    if (PE_parse_boot_argn("broadtekclass", &wanted, sizeof(wanted))) {
        if (classIsTaken(wanted)) {
            SYSLOG("broadtek", "usbclass: 0x%02x is itself one of the classes "
                   "bluetoothd takes; ignoring it and using 0x00", wanted);
        } else {
            g_new_class = wanted;
        }
    }

    g_report[0] = '\0';
    publish("broadtek-usbclass", g_dry_run ? "dry run" : "waiting for a device");

    IONotifier *n = IOService::addMatchingNotification(
        gIOMatchedNotification, IOService::serviceMatching("IOUSBHostDevice"),
        onUsbDevice, nullptr, nullptr, 0);

    SYSLOG("broadtek", "usbclass: %s, replacement class 0x%02x, notifier %s",
           g_dry_run ? "dry run" : "active", g_new_class,
           n != nullptr ? "installed" : "FAILED");
    if (n == nullptr) publish("broadtek-usbclass", "notifier failed");
}
