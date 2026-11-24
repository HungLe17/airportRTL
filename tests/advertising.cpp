// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth/AdvertisingSets.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

using Packet = std::vector<uint8_t>;

int main() {
    airportRTL::AdvertisingSets controller;
    airportRTL::AdvertisingSets otherController;
    const Packet address{0x05, 0x20, 6, 1, 2, 3, 4, 5, 0x48};
    const Packet enable{0x39, 0x20, 10, 1, 2, 0, 0, 0, 0, 0xef, 0, 0, 0};
    auto complete = [&](const Packet &p, uint8_t status = 0) {
        Packet event{0x0e, 4, 1, p[0], p[1], status};
        if (p[0] == 0x36 && p[1] == 0x20) {
            event[1] = 5;
            event.push_back(4);
        }
        controller.observeHostCompletion(p.data(), p.size(), event.data(), event.size());
    };
    auto parameters = [&](uint8_t handle, uint8_t addressType) {
        Packet p(28);
        p[0] = 0x36; p[1] = 0x20; p[2] = 25;
        p[3] = handle; p[13] = addressType;
        complete(p);
    };
    auto check = [&](const Packet &p, std::initializer_list<uint8_t> expected,
                     uint8_t firstAddressByte = 1) {
        std::vector<uint8_t> handles;
        assert(controller.beforeEnable(p.data(), p.size(), [&](const uint8_t *cmd, size_t n,
                                                                uint8_t *reply, size_t capacity) {
            assert(n == 10 && cmd[0] == 0x35 && cmd[1] == 0x20 && cmd[2] == 7);
            assert(cmd[4] == firstAddressByte && !memcmp(cmd + 5, address.data() + 4, 5));
            handles.push_back(cmd[3]);
            const uint8_t ack[] = {0x0e, 4, 1, 0x35, 0x20, 0};
            assert(capacity == sizeof(ack));
            memcpy(reply, ack, sizeof(ack));
            return sizeof(ack);
        }));
        assert(handles == std::vector<uint8_t>(expected));
    };

    parameters(0, 1);
    parameters(0xef, 1);
    complete(address, 0x0c);
    check(enable, {});
    for (const Packet &event : {Packet{0x0e, 4, 1, 6, 0x20, 0},
                                Packet{0x0f, 4, 0, 1, 5, 0x20},
                                Packet{0x0e, 4, 1, 5, 0x20}}) {
        controller.observeHostCompletion(address.data(), address.size(), event.data(), event.size());
        check(enable, {});
    }
    complete(address);
    check(enable, {0, 0xef});
    parameters(0xef, 0);
    check(enable, {0});
    parameters(0xef, 1);
    complete({0x35, 0x20, 7, 0xef, 1, 2, 3, 4, 5, 0x42});
    check(enable, {0});
    Packet rotated = address;
    rotated[3] = 42;
    complete(rotated);
    check(enable, {0}, 42);
    complete({0x3c, 0x20, 1, 0xef});
    parameters(0xef, 1);
    check(enable, {0, 0xef}, 42);
    for (const Packet &event : {Packet{}, Packet{0x0e, 4, 0, 0x35, 0x20, 0},
                                Packet{0x0e, 4, 1, 0x35, 0x20, 0x0c},
                                Packet{0x0e, 4, 1, 0x36, 0x20, 0},
                                Packet{0x0f, 4, 0, 1, 0x35, 0x20}}) {
        size_t submissions = 0;
        assert(!controller.beforeEnable(enable.data(), enable.size(),
            [&](const uint8_t *, size_t, uint8_t *reply, size_t capacity) {
                ++submissions;
                assert(event.size() <= capacity);
                if (!event.empty()) memcpy(reply, event.data(), event.size());
                return event.size();
            }));
        assert(submissions == 1);
    }
    assert(!controller.beforeEnable(enable.data(), enable.size(),
        [](const uint8_t *cmd, size_t, uint8_t *reply, size_t) {
            uint8_t ack[] = {0x0e, 4, 1, 0x35, 0x20, 0};
            if (cmd[3] == 0xef) ack[5] = 0x0c;
            memcpy(reply, ack, sizeof(ack));
            return sizeof(ack);
        }));
    check(enable, {0, 0xef}, 42);
    for (size_t n = 0; n < enable.size(); ++n)
        check(Packet(enable.begin(), enable.begin() + n), {});
    Packet invalid = enable;
    invalid[9] = 0xf0;
    check(invalid, {});
    invalid[9] = invalid[5];
    check(invalid, {});
    invalid = enable; invalid[3] = 0;
    check(invalid, {});
    complete({0x3d, 0x20, 0});
    check(enable, {});
    parameters(0, 1);
    check(enable, {0}, 42);
    assert(otherController.beforeEnable(enable.data(), enable.size(), [](const uint8_t *, size_t, uint8_t *, size_t) {
        return size_t{0};
    }));
    complete({0x03, 0x0c, 0}, 1);
    check(enable, {0}, 42);
    complete({0x03, 0x0c, 0});
    parameters(0, 1);
    check(enable, {});
    puts("Advertising set addressing tests passed");
}
