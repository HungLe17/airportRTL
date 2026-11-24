// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/bluetooth/BtProtocol.hpp"
#include "../../src/bluetooth/RtlFwParse.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
using Bytes = std::vector<uint8_t>;
using namespace RTW89BT;
static void put32(Bytes &b, size_t at, uint32_t n) { for (unsigned i = 0; i < 4; ++i) b[at+i] = uint8_t(n >> (i*8)); }

static void advertising() {
    AdvertisingCapabilities caps;CHECK(!caps.ready());
    const uint16_t opcodes[]={0x1002,0x2003,0x203a,0x203b};
    Bytes replies[]={Bytes(65),Bytes(9),Bytes{0,0x72,6},Bytes{0,4}};
    replies[0][37]=0xfe;replies[0][38]=3;replies[1][2]=0x10;
    for(unsigned i=0;i<4;++i) {
        auto &reply=replies[i];const auto before=caps;
        for(size_t n=0;n<reply.size();++n) {
            CHECK(!caps.apply(opcodes[i],reply.data(),n));
            CHECK(caps.readMask==before.readMask && caps.sets==before.sets &&
                  caps.maxDataLength==before.maxDataLength && caps.extended==before.extended &&
                  caps.commands==before.commands);
        }
        reply[0]=1;CHECK(!caps.apply(opcodes[i],reply.data(),reply.size()));reply[0]=0;
        CHECK(!caps.apply(opcodes[i],nullptr,reply.size()));
        CHECK(caps.apply(opcodes[i],reply.data(),reply.size()));
        CHECK(caps.ready()==(i==3));
    }
    for(uint8_t n:{uint8_t(0),uint8_t(1),uint8_t(3),uint8_t(241),uint8_t(255)}) {
        auto c=caps;c.sets=n;CHECK(!c.ready());
    }
    for(uint16_t n:{uint16_t(0),uint16_t(30),uint16_t(1651),uint16_t(65535)}) {
        auto c=caps;c.maxDataLength=n;CHECK(!c.ready());
    }
    for(unsigned bit=1;bit<8;++bit) {
        auto c=caps;auto reply=replies[0];reply[37]&=uint8_t(~(1u<<bit));
        CHECK(c.apply(0x1002,reply.data(),reply.size()));CHECK(!c.ready());
    }
    for(unsigned bit=0;bit<2;++bit) {
        auto c=caps;auto reply=replies[0];reply[38]&=uint8_t(~(1u<<bit));
        CHECK(c.apply(0x1002,reply.data(),reply.size()));CHECK(!c.ready());
    }
    auto c=caps;replies[1][2]=0;
    CHECK(c.apply(0x2003,replies[1].data(),replies[1].size()));CHECK(!c.ready());
    CHECK(!c.apply(0xffff,replies[1].data(),replies[1].size()));

}

static void replies() {
    const auto *profile = findFirmwareProfile(0x5d, 0x8852, 0xb, 0xb);
    CHECK(profile && profile->projectId == 20 && profile->readRomVersion);
    CHECK(!strcmp(profile->filename, "rtl8852bu_fw.bin"));
    CHECK(!findFirmwareProfile(0xf, 0x8852, 0xb, 0xb));
    CHECK(!findFirmwareProfile(0x5d, 0x8852, 0xc, 0xc));
    CHECK(!findFirmwareProfile(0x5d, 0x8852, 0x87, 0xc));
    CHECK(findRunningFirmwareProfile(0x5d, 0xfd78, 0x127c, 0xb) == profile);
    CHECK(!findFirmwareProfile(0x5d, 0xfd78, 0x127c, 0xb));
    CHECK(!findRunningFirmwareProfile(0x5d, 0x8852, 0xb, 0xb));
    CHECK(!findRunningFirmwareProfile(0x5d, 0xfd78, 0x127d, 0xb));
    CHECK(!findRunningFirmwareProfile(0x5d, 0x127c, 0xfd78, 0xb));
    CHECK(!findRunningFirmwareProfile(0x5d, 0xfd78, 0x127c, 0xa));
    CHECK(!findRunningFirmwareProfile(0xf, 0xfd78, 0x127c, 0xb));
    const uint8_t *p = nullptr; size_t length = 0;
    Bytes event{0x0e, 5, 1, 0x20, 0xfc, 0, 0x81};
    CHECK(completion(event.data(), event.size(), 0xfc20, p, length) == Reply::Complete);
    CHECK(length == 2 && p[1] == 0x81);
    for (size_t n = 0; n < event.size(); ++n)
        CHECK(completion(event.data(), n, 0xfc20, p, length) == Reply::Malformed);
    CHECK(completion(nullptr, 0, 0, p, length) == Reply::Malformed);
    CHECK(completion(event.data(), event.size(), 0x1001, p, length) == Reply::Unrelated);
    for (unsigned status = 1; status < 256; ++status) {
        event[5] = uint8_t(status);
        CHECK(completion(event.data(), event.size(), 0xfc20, p, length) == Reply::Failed);
    }
    event = {0x0f, 4, 0, 1, 0x20, 0xfc};
    CHECK(completion(event.data(), event.size(), 0xfc20, p, length) == Reply::Pending);
    event[2] = 1;
    CHECK(completion(event.data(), event.size(), 0xfc20, p, length) == Reply::Failed);
    event[1] = 5;
    CHECK(completion(event.data(), event.size(), 0xfc20, p, length) == Reply::Malformed);

    CommandFlow flow;
    CHECK(flow.ready() && !flow.pending());
    CHECK(flow.begin(0x203a) && flow.pending());
    CHECK(!flow.begin(0x203b) && !flow.ready());
    auto observe = [&](const Bytes &e) { return flow.observe(e.data(), e.size(), p, length); };
    // A credit notification, malformed reply or unexpected completion cannot
    // settle a capability request whose response is still outstanding.
    CHECK(observe({0x0e, 3, 1, 0, 0}) == Reply::Unrelated);
    CHECK(observe({0x0e, 3, 1, 0x3a, 0x20}) == Reply::Malformed);
    CHECK(observe({0x0e, 4, 2, 0x3b, 0x20, 0}) == Reply::Unrelated);
    CHECK(observe({0x0f, 4, 0, 1, 0x3a, 0x20}) == Reply::Pending);
    CHECK(flow.pending() && !flow.ready());
    CHECK(observe({0x0e, 6, 0, 0x3a, 0x20, 0, 0, 4}) == Reply::Complete);
    CHECK(length == 3 && !flow.pending() && !flow.ready());
    CHECK(!flow.begin(0x203b));
    CHECK(observe({0x0e, 3, 1, 0, 0}) == Reply::Unrelated);
    CHECK(flow.ready() && flow.begin(0x203b));
    CHECK(observe({0x0f, 4, 0x01, 1, 0x3b, 0x20}) == Reply::Failed);
    CHECK(flow.ready() && !flow.pending());
}

static void fragments() {
    for (size_t size : {1u, 251u, 252u, 253u, 504u, 32004u, 32256u, 65536u}) {
        Bytes source(size), reconstructed;
        for (size_t n = 0; n < size; ++n) source[n] = uint8_t(n * 17);
        uint8_t command[253];
        const size_t count = rtlFragmentCount(uint32_t(size));
        for (size_t n = 0; n < count; ++n) {
            size_t bytes = fragment(source.data(), source.size(), n, command, sizeof(command));
            CHECK(bytes >= 1 && bytes <= 253);
            CHECK(bool(command[0] & 0x80) == (n == count - 1));
            if (n == 127) CHECK((command[0] & 0x7f) == 127);
            if (n == 128) CHECK((command[0] & 0x7f) == 1);
            reconstructed.insert(reconstructed.end(), command + 1, command + bytes);
        }
        CHECK(source == reconstructed);
        CHECK(!fragment(source.data(), size, count, command, sizeof(command)));
        CHECK(!fragment(source.data(), size, 0, command, sizeof(command)-1));
    }
}

static Bytes firmware() {
    // Two sections: ROM 1 snippets, then a keyed security header. Priorities
    // require output B before A, followed by C only for key 7.
    Bytes f(20, 0); memcpy(f.data(), "RTBTCore", 8); put32(f, 16, 2);
    auto section = [&](uint32_t opcode, uint8_t priority, uint8_t key, uint8_t value) {
        size_t at = f.size(); f.resize(at + 21);
        put32(f, at, opcode); put32(f, at+4, 13);
        f[at+8] = 1; f[at+12] = 1; f[at+13] = priority; f[at+14] = key;
        put32(f, at+16, 1); f[at+20] = value;
    };
    section(RTL_SEC_SNIPPETS, 2, 0, 'A');
    section(RTL_SEC_SNIPPETS, 1, 0, 'B');
    section(RTL_SEC_SECURITY_HEADER, 3, 7, 'C');
    put32(f, 16, 3);
    f.insert(f.end(), {20, 1, 0, 0x51, 4, 0xfd, 0x77});
    return f;
}

static void parser() {
    auto f = firmware(); RtlSubsecRef scratch[8]; uint8_t out[16] = {};
    CHECK(rtlFindProjectId(f.data(), uint32_t(f.size())) == 20);
    CHECK(rtlParseFirmwareV2(f.data(), uint32_t(f.size()), 0, 0, scratch, 8, out, sizeof(out)) == 2);
    CHECK(out[0] == 'B' && out[1] == 'A');
    CHECK(rtlParseFirmwareV2(f.data(), uint32_t(f.size()), 0, 7, scratch, 8, out, sizeof(out)) == 3);
    CHECK(out[0] == 'B' && out[1] == 'A' && out[2] == 'C');
    CHECK(!rtlParseFirmwareV2(f.data(), uint32_t(f.size()), 1, 0, scratch, 8, out, sizeof(out)));
    CHECK(!rtlParseFirmwareV2(f.data(), uint32_t(f.size()), 0, 0, scratch, 1, out, sizeof(out)));
    for (size_t n = 0; n < f.size(); ++n) {
        CHECK(rtlFindProjectId(f.data(), uint32_t(n)) < 0);
        CHECK(!rtlParseFirmwareV2(f.data(), uint32_t(n), 0, 0, scratch, 8, out, sizeof(out)));
    }
    auto broken = f; put32(broken, 16, 4); memset(out, 0xcc, sizeof(out));
    CHECK(!rtlParseFirmwareV2(broken.data(), uint32_t(broken.size()), 0, 0, scratch, 8, out, sizeof(out)));
    CHECK(out[0] == 0xcc); // no partial upload after a corrupt trailing section
    broken = f; put32(broken, 24, 0xffffffff);
    CHECK(!rtlParseFirmwareV2(broken.data(), uint32_t(broken.size()), 0, 0, scratch, 8, out, sizeof(out)));
    broken = f; broken[broken.size()-6] = 255;
    CHECK(rtlFindProjectId(broken.data(), uint32_t(broken.size())) < 0);
    CHECK(!rtlParseFirmwareV2(nullptr, 100, 0, 0, scratch, 8, out, sizeof(out)));
}

static void realFirmware(const char *file) {
    std::ifstream stream(file, std::ios::binary); CHECK(stream.good());
    Bytes f((std::istreambuf_iterator<char>(stream)), {}), out(f.size());
    CHECK(rtlFindProjectId(f.data(), uint32_t(f.size())) == 20);
    std::vector<RtlSubsecRef> scratch(512);
    unsigned usable = 0;
    for (unsigned rom = 0; rom < 8; ++rom) {
        const auto length = rtlParseFirmwareV2(f.data(), uint32_t(f.size()), uint8_t(rom), 0,
                                               scratch.data(), 512, out.data(), uint32_t(out.size()));
        CHECK(length <= f.size());
        if (length) { ++usable; std::printf("RTL8852B fixture: ROM %u, payload %u bytes\n", rom, length); }
    }
    CHECK(usable > 0);
    // All truncated prefixes must be rejected, including copies ending in
    // the middle of a section/subsection or the extension trailer.
    for (size_t n = 0; n < f.size(); ++n)
        CHECK(!rtlParseFirmwareV2(f.data(), uint32_t(n), 0, 0, scratch.data(), 512, out.data(), uint32_t(out.size())));
}

int main(int argc, char **argv) {
    CHECK(argc == 2); replies(); advertising();
    fragments(); parser(); realFirmware(argv[1]);
    std::printf("Bluetooth portable tests passed (%u checks)\n", checks);
}
