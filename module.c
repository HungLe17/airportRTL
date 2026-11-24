// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
#include <mach/mach_types.h>

extern kern_return_t _start(kmod_info_t *, void *);
extern kern_return_t _stop(kmod_info_t *, void *);

// Expand the build identifier before KMOD_EXPLICIT_DECL stringifies it.
#define DECLARE_MODULE(identifier) KMOD_EXPLICIT_DECL(identifier, MODULE_VERSION, _start, _stop)
DECLARE_MODULE(MODULE_ID)
#if defined(MODULE_START) && defined(MODULE_STOP)
extern kern_return_t MODULE_START(kmod_info_t *, void *);
extern kern_return_t MODULE_STOP(kmod_info_t *, void *);
__private_extern__ kmod_start_func_t *_realmain = MODULE_START;
__private_extern__ kmod_stop_func_t *_antimain = MODULE_STOP;
#else
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
#endif
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
