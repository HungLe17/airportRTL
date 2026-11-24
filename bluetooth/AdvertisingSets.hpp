// SPDX-License-Identifier: GPL-3.0-or-later
// The per-set address behavior follows BroadTek v0.2.0 (see BroadTek-LICENSE.txt).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../src/bluetooth/BtProtocol.hpp"

namespace airportRTL {

// One instance per controller, used by a serialized HCI command dispatcher.
// Accept USB HCI command packets (no H4 prefix). Commit configuration only
// after the controller acknowledges the host's command.
class AdvertisingSets {
    static constexpr unsigned handleCount = 0xf0;
    struct Handles {
        uint8_t bits[handleCount / 8]{};
        bool contains(uint8_t h) const { return bits[h / 8] & (1u << (h % 8)); }
        void set(uint8_t h, bool present) {
            const uint8_t bit = 1u << (h % 8);
            if (present) bits[h / 8] |= bit;
            else bits[h / 8] &= ~bit;
        }
    };

    Handles randomSets{}, hostAddressed{};
    uint8_t randomAddress[6]{};
    bool haveAddress = false;

    static bool valid(const uint8_t *packet, size_t size) {
        return packet && size >= 3 && size == 3u + packet[2];
    }

public:
    // The transport keeps the associated host command until Command Complete.
    // An injected command's completion must not establish host ownership.
    void observeHostCompletion(const uint8_t *packet, size_t size,
                               const uint8_t *event, size_t eventSize) {
        if (!valid(packet, size)) return;
        const uint16_t opcode = packet[0] | (uint16_t(packet[1]) << 8);
        const uint8_t *result;
        size_t resultSize;
        if (RTW89BT::completion(event, eventSize, opcode, result, resultSize) !=
            RTW89BT::Reply::Complete) return;
        // Extended parameters also return the selected TX power.
        if (resultSize != (opcode == 0x2036 ? 2u : 1u)) return;
        const uint8_t *p = packet + 3;
        switch (opcode) {
        case 0x0c03: // HCI Reset
            if (size == 3) *this = {};
            break;
        case 0x2005: // LE Set Random Address
            if (size == 9) {
                memcpy(randomAddress, p, sizeof(randomAddress));
                haveAddress = true;
            }
            break;
        case 0x2036: // LE Set Extended Advertising Parameters
            if (size == 28 && p[0] < handleCount)
                randomSets.set(p[0], p[10] == 1);
            break;
        case 0x2035: // Host explicitly set this advertising set's address.
            if (size == 10 && p[0] < handleCount) hostAddressed.set(p[0], true);
            break;
        case 0x203c: // LE Remove Advertising Set
            if (size == 4 && p[0] < handleCount) {
                randomSets.set(p[0], false);
                hostAddressed.set(p[0], false);
            }
            break;
        case 0x203d: // LE Clear Advertising Sets preserves the controller address.
            if (size == 3) {
                randomSets = {};
                hostAddressed = {};
            }
            break;
        }
    }

    // transact writes the controller's matching reply into the supplied buffer
    // and returns its length (zero on transport failure). The original enable
    // stays with the caller until each preceding address command completes.
    // Unrecognized or malformed host requests pass through to native handling.
    template <class Send>
    bool beforeEnable(const uint8_t *packet, size_t size, Send transact) const {
        if (!valid(packet, size) || size < 5 || packet[0] != 0x39 ||
            packet[1] != 0x20 || packet[3] != 1 || !haveAddress) return true;
        const unsigned count = packet[4];
        if (size != 5u + 4u * count) return true;
        Handles seen{};
        for (unsigned i = 0; i < count; ++i) {
            const uint8_t h = packet[5 + 4 * i];
            if (h >= handleCount || seen.contains(h)) return true;
            seen.set(h, true);
        }
        uint8_t addressCommand[10] = {0x35, 0x20, 7, 0};
        memcpy(addressCommand + 4, randomAddress, sizeof(randomAddress));
        for (unsigned i = 0; i < count; ++i) {
            const uint8_t h = packet[5 + 4 * i];
            if (randomSets.contains(h) && !hostAddressed.contains(h)) {
                addressCommand[3] = h;
                uint8_t reply[6]{};
                const size_t n = transact(addressCommand, sizeof(addressCommand), reply, sizeof(reply));
                const uint8_t *result;
                size_t resultSize;
                if (n != sizeof(reply) || !reply[2] ||
                    RTW89BT::completion(reply, n, 0x2035, result, resultSize)
                        != RTW89BT::Reply::Complete) return false;
            }
        }
        return true;
    }
};
} // namespace airportRTL
