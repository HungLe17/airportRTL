// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <libkern/c++/OSData.h>
#include <libkern/libkern.h>
// Uncompressed, hash-verified firmware is generated at build time. Avoid an
// estimated decompression allocation and additional kernel zlib imports.
extern const unsigned char rtw89_bt_firmware[];
extern const unsigned int rtw89_bt_firmware_size;
static inline OSData *getFWDescByName(const char *name) {
    return name && !strcmp(name, "rtl8852bu_fw.bin") ?
        OSData::withBytes(rtw89_bt_firmware, rtw89_bt_firmware_size) : nullptr;
}
