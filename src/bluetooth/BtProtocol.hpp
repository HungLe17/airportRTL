// SPDX-License-Identifier: GPL-3.0-or-later
// Hardware-independent checks shared by the USB loader and its host tests.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace RTW89BT {
constexpr size_t MaxFirmware = 1024 * 1024;
constexpr size_t FragmentSize = 252;
inline uint16_t le16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
struct FirmwareProfile {
    uint16_t manufacturer, lmpSubversion, hciRevision;
    uint16_t runningLmpSubversion, runningHciRevision;
    uint8_t hciVersion, projectId;
    bool readRomVersion;
    const char *filename;
    const char *chipName;
};

inline constexpr FirmwareProfile firmwareProfiles[] = {
    // The running revision pair is from the ECO 2 patch in the pinned
    // image and matches the HCI version reported on USB 13d3:3571.
    {0x005d, 0x8852, 0x000b, 0xfd78, 0x127c, 0x0b, 20, true,
     "rtl8852bu_fw.bin", "rtl8852bu"},
};

// Several 8852 variants report the same LMP subversion. Firmware selection
// requires the USB controller's complete version tuple, independent of Wi-Fi PCI ID.
inline const FirmwareProfile *findFirmwareProfile(uint16_t manufacturer, uint16_t lmp,
                                                  uint16_t revision, uint8_t version) {
    for (const auto &profile : firmwareProfiles) {
        if (profile.manufacturer == manufacturer && profile.lmpSubversion == lmp &&
            profile.hciRevision == revision && profile.hciVersion == version)
            return &profile;
    }
    return nullptr;
}

inline const FirmwareProfile *findRunningFirmwareProfile(uint16_t manufacturer, uint16_t lmp,
                                                         uint16_t revision, uint8_t version) {
    for (const auto &profile : firmwareProfiles) {
        if (profile.manufacturer == manufacturer && profile.runningLmpSubversion == lmp &&
            profile.runningHciRevision == revision && profile.hciVersion == version)
            return &profile;
    }
    return nullptr;
}
enum class Reply { Malformed, Unrelated, Pending, Complete, Failed };
// Standard HCI replies, including their status byte. A capability proof must
// come from actual Command Complete events, never a host API's return value.
struct AdvertisingCapabilities {
    uint8_t readMask = 0, sets = 0;
    uint16_t maxDataLength = 0;
    bool extended = false, commands = false;
    bool apply(uint16_t opcode, const uint8_t *p, size_t n) {
        if (!p || !n || p[0]) return false;
        switch (opcode) {
        case 0x1002: // Read Local Supported Commands; Core octets 36/37
            if (n != 65) return false;
            commands = (p[1+36] & 0xfe) == 0xfe && (p[1+37] & 3) == 3;
            readMask |= 1; return true;
        case 0x2003: // LE Read Local Supported Features; bit 12
            if (n != 9) return false;
            extended = (p[2] & 0x10) != 0;
            readMask |= 2; return true;
        case 0x203a:
            if (n != 3) return false;
            maxDataLength = le16(p+1); readMask |= 4; return true;
        case 0x203b:
            if (n != 2) return false;
            sets = p[1]; readMask |= 8; return true;
        default: return false;
        }
    }
    bool ready() const {
        // The selected host uses handles 0..3. Do not enable that policy on
        // a controller that cannot actually provide four advertising sets.
        return readMask == 15 && extended && commands &&
            maxDataLength >= 31 && maxDataLength <= 1650 && sets >= 4 && sets <= 240;
    }
};
// USB interrupt events have no H4 packet-type byte. Reject truncation instead
// of accepting a successful prefix of an incomplete controller response.
inline Reply completion(const uint8_t *event, size_t n, uint16_t opcode,
                        const uint8_t *&params, size_t &length, uint8_t *credits = nullptr) {
    params = nullptr; length = 0;
    if (!event || n < 2 || n != size_t(event[1]) + 2) return Reply::Malformed;
    if (event[0] == 0x0f) {
        if (n != 6) return Reply::Malformed;
        if (credits) *credits = event[3];
        if (!opcode || le16(event + 4) != opcode) return Reply::Unrelated;
        return event[2] ? Reply::Failed : Reply::Pending;
    }
    if (event[0] != 0x0e) return Reply::Unrelated;
    if (n < 5) return Reply::Malformed;
    const auto completedOpcode = le16(event + 3);
    if (completedOpcode && n < 6) return Reply::Malformed;
    if (credits) *credits = event[2];
    if (!completedOpcode || completedOpcode != opcode) return Reply::Unrelated;
    params = event + 5; length = n - 5;
    return params[0] ? Reply::Failed : Reply::Complete;
}

// Firmware setup uses one outstanding command. When a submission or its
// reply times out, only a matching terminal event can settle that command.
class CommandFlow {
public:
    bool ready() const { return !outstanding && credits; }
    bool pending() const { return outstanding != 0; }
    bool begin(uint16_t opcode) {
        if (!opcode || !ready()) return false;
        outstanding = opcode;
        --credits;
        return true;
    }
    Reply observe(const uint8_t *event, size_t size, const uint8_t *&params, size_t &length) {
        const auto reply = completion(event, size, outstanding, params, length, &credits);
        if (reply == Reply::Complete || reply == Reply::Failed)
            outstanding = 0;
        return reply;
    }

private:
    // The host can initially send one command after power-on.
    uint8_t credits = 1;
    uint16_t outstanding = 0;
};

inline size_t fragment(const uint8_t *firmware, size_t length, size_t number,
                       uint8_t *out, size_t capacity) {
    if (!firmware || !length || length > MaxFirmware || number > length / FragmentSize ||
        !out || capacity < FragmentSize + 1) return 0;
    const size_t offset = number * FragmentSize;
    const size_t remaining = length - offset;
    const size_t count = remaining < FragmentSize ? remaining : FragmentSize;
    // Realtek indices wrap 0x7f -> 1. The final command carries bit 7;
    // exact multiples require a final, index-only command.
    uint8_t index = number ? uint8_t((number - 1) % 127 + 1) : 0;
    if (number == length / FragmentSize) index |= 0x80;
    out[0] = index;
    if (count) memcpy(out + 1, firmware + offset, count);
    return count + 1;
}
} // namespace RTW89BT
