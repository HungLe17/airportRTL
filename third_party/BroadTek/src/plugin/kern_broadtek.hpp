/* SPDX-License-Identifier: MIT */
#ifndef KERN_BROADTEK_HPP
#define KERN_BROADTEK_HPP

#include <Headers/kern_patcher.hpp>

/* Not named BroadTek: Lilu's plugin_start.hpp already defines a class by that
 * name (the PRODUCT_NAME IOService). */
/* Called from the config service (kern_config.cpp) when userspace hands a
 * key down. The blob is address type (1), identity address (6) and IRK (16)
 * in HCI byte order. Returns true if it was stored. The key is never logged
 * and never published. */
bool brtkInstallKey(const uint8_t *blob, size_t len);

/* Called from the config service when userspace hands down an identity and
 * the private address it is currently using, with no key. The blob is address
 * type (1), identity address (6) and private address (6), in HCI byte order.
 * Returns true if it was stored. */
bool brtkInstallMap(const uint8_t *blob, size_t len);

/* Move finished ACL-side captures into the resolving list. */
int brtkPromoteCaptures();

class BRTK {
public:
    void init();
private:
    void onPatcherLoad(KernelPatcher &patcher);
    void onKextLoad(KernelPatcher &patcher, size_t id, mach_vm_address_t slide, size_t size);
    void installRoutes(KernelPatcher &patcher, size_t id, mach_vm_address_t start,
                       size_t size, const char *via);
};

#endif
