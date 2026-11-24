/* SPDX-License-Identifier: MIT */
#ifndef KERN_CONFIG_HPP
#define KERN_CONFIG_HPP

#include <IOKit/IOService.h>

/* The way a key gets into the kext.
 *
 * macOS never gives this controller an IRK through HCI (docs/STATE.md §3), so
 * one has to be handed down from userspace: read out of the keychain by
 * tools/irk_install.c and written here.
 *
 * This is a property write rather than an IOUserClient, deliberately. A
 * single one-way message needs no session, no external-method dispatch and no
 * user-client class in the Info.plist, and setProperties is write-only by
 * nature: data goes in, a status comes back, and there is no call that could
 * return a key. The whole channel is one method.
 *
 * What arrives is never stored as a property. Publishing it would put a
 * cryptographic key in the IO registry, which any process can read -- the
 * same rule the capture path follows. */
class BroadTekConfig : public IOService {
    OSDeclareDefaultStructors(BroadTekConfig);

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
};

#endif
