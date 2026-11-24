//
//  RTW89BTFirmwareCore.cpp
//  RealtekBluetoothFirmware
//
//  Created by theVakhovskeIsTaken on 2026/8/9.
//  Copyright © 2026 thegwchr. All rights reserved.
//

#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>

#include "Log.h"
#include "BtRtl.hpp"
#include "FwData.h"
#include "RtlFwParse.h"
#include "BtProtocol.hpp"
#include <kern/clock.h>

#define super OSObject
OSDefineMetaClassAndStructors(RTW89BTFirmwareCore, OSObject)

bool RTW89BTFirmwareCore::
initWithDevice(IOService *client, IOUSBHostDevice *dev)
{
    XYLog("%s\n", __PRETTY_FUNCTION__);
    if (!super::init())
        return false;

    m_icInfo = NULL;
    m_romVersion = 0;
    m_keyId = 0;
    m_fwName[0] = '\0';
    m_advertising = {};
    m_hciFlow = {};

    m_pRTW89BTTransport = new RTW89BTTransport();
    if (!m_pRTW89BTTransport || !m_pRTW89BTTransport->init(client, dev))
        return false;
    if (!m_pRTW89BTTransport->initConfiguration())
        return false;
    if (!m_pRTW89BTTransport->findInterface())
        return false;
    if (!m_pRTW89BTTransport->findPipes())
        return false;
    return true;
}

void RTW89BTFirmwareCore::
free()
{
    XYLog("%s\n", __PRETTY_FUNCTION__);
    OSSafeReleaseNULL(m_pRTW89BTTransport);
    super::free();
}

uint8_t RTW89BTFirmwareCore::releaseUSBForHandoff()
{
    return m_pRTW89BTTransport ? m_pRTW89BTTransport->releaseForHandoff() : 0;
}

bool RTW89BTFirmwareCore::
getFirmwareName(char *fwname, size_t len)
{
    if (!fwname || len == 0)
        return false;
    strlcpy(fwname, m_fwName[0] ? m_fwName : "rtl_unknown", len);
    return true;
}

void RTW89BTFirmwareCore::readAdvertisingCapabilities()
{
    m_advertising = {};
    const uint16_t opcodes[] = {0x1002, 0x2003, 0x203a, 0x203b};
    for (uint16_t opcode : opcodes) {
        if (opcode >= 0x203a && (!m_advertising.commands || !m_advertising.extended)) break;
        const uint8_t *params = nullptr;
        uint32_t length = 0;
        if (!hciCmd(opcode, nullptr, 0, &params, &length, HCI_CMD_TIMEOUT) ||
            !m_advertising.apply(opcode, params, length)) {
            XYLog("advertising capability query %04x failed; policy proof withheld\n", opcode);
            return;
        }
    }
    XYLog("advertising capabilities: reads=%u extended=%u commands=%u max-data=%u sets=%u proof=%u\n",
          m_advertising.readMask, m_advertising.extended, m_advertising.commands,
          m_advertising.maxDataLength, m_advertising.sets, m_advertising.ready());
}

static uint32_t remainingMilliseconds(uint64_t deadline)
{
    uint64_t now;
    clock_get_uptime(&now);
    if (now >= deadline)
        return 0;
    uint64_t remainingNS;
    absolutetime_to_nanoseconds(deadline - now, &remainingNS);
    return uint32_t((remainingNS + 999999) / 1000000);
}

IOReturn RTW89BTFirmwareCore::readHCIEvent(uint32_t &size, uint64_t deadline)
{
    const auto remaining = remainingMilliseconds(deadline);
    if (!remaining)
        return kIOReturnTimeout;
    return m_pRTW89BTTransport->interruptPipeRead(m_evtBuf, sizeof(m_evtBuf), &size, remaining);
}

bool RTW89BTFirmwareCore::waitForCommandCredit(uint64_t deadline)
{
    // After a failed optional query, a late matching terminal reply may
    // still clear the outstanding command. Send nothing until it does.
    for (unsigned i = 0; i < 16 && !m_hciFlow.ready(); ++i) {
        uint32_t size = 0;
        if (readHCIEvent(size, deadline) != kIOReturnSuccess)
            return false;
        const uint8_t *params = nullptr;
        size_t length = 0;
        if (m_hciFlow.observe(m_evtBuf, size, params, length) == RTW89BT::Reply::Malformed)
            return false;
    }
    return m_hciFlow.ready();
}

bool RTW89BTFirmwareCore::
hciCmd(uint16_t opcode, const void *param, uint8_t plen,
       const uint8_t **outParams, uint32_t *outParamLen, int timeout)
{
    if (timeout <= 0 || (plen && !param)) return false;
    uint64_t deadline;
    clock_interval_to_deadline(timeout, kMillisecondScale, &deadline);
    if (!waitForCommandCredit(deadline)) {
        XYLog("HCI command 0x%04x has no command credit\n", opcode);
        return false;
    }
    HciCommandHdr *cmd = reinterpret_cast<HciCommandHdr *>(m_cmdBuf);
    cmd->opcode = OSSwapHostToLittleInt16(opcode);
    cmd->len = plen;
    if (plen) memcpy(cmd->data, param, plen);
    const auto remaining = remainingMilliseconds(deadline);
    if (!remaining || !m_hciFlow.begin(opcode))
        return false;
    IOReturn ret = m_pRTW89BTTransport->sendHCIRequest(cmd, remaining);
    if (ret != kIOReturnSuccess) {
        XYLog("HCI send 0x%04x failed 0x%x\n", opcode, ret);
        return false;
    }
    // Each transaction is bounded; only controller completions count as
    // success. No synthetic completion, feature mask, address or key.
    for (unsigned i = 0; i < 16; ++i) {
        uint32_t size = 0;
        ret = readHCIEvent(size, deadline);
        if (ret != kIOReturnSuccess) {
            XYLog("HCI read 0x%04x failed 0x%x\n", opcode, ret);
            return false;
        }
        const uint8_t *params = nullptr;
        size_t length = 0;
        const auto reply = m_hciFlow.observe(m_evtBuf, size, params, length);
        if (reply == RTW89BT::Reply::Malformed || reply == RTW89BT::Reply::Failed) {
            XYLog("HCI command 0x%04x rejected/malformed (event 0x%02x, status 0x%02x)\n",
                  opcode, size ? m_evtBuf[0] : 0,
                  params ? params[0] : (size == 6 && m_evtBuf[0] == 0x0f ? m_evtBuf[2] : 0xff));
            return false;
        }
        if (reply != RTW89BT::Reply::Complete) continue;
        if (outParams) *outParams = params;
        if (outParamLen) *outParamLen = uint32_t(length);
        return true;
    }
    XYLog("HCI command 0x%04x exhausted event limit\n", opcode);
    return false;
}

bool RTW89BTFirmwareCore::
readLocalVersion(HciRpReadLocalVersion *out)
{
    const uint8_t *p = NULL;
    uint32_t plen = 0;
    if (!hciCmd(HCI_OP_READ_LOCAL_VERSION, NULL, 0, &p, &plen, HCI_INIT_TIMEOUT))
        return false;
    if (plen != 9) {
        XYLog("read local version: unexpected event length (%u)\n", plen);
        return false;
    }
    if (p[0] != 0) {
        XYLog("read local version: status 0x%02x\n", p[0]);
        return false;
    }
    out->status       = p[0];
    out->hci_ver      = p[1];
    out->hci_rev      = OSReadLittleInt16(p, 2);
    out->lmp_ver      = p[4];
    out->manufacturer = OSReadLittleInt16(p, 5);
    out->lmp_subver   = OSReadLittleInt16(p, 7);
    return true;
}

bool RTW89BTFirmwareCore::
readRomVersion(uint8_t *version)
{
    const uint8_t *p = NULL;
    uint32_t plen = 0;
    if (!hciCmd(RTL_OP_READ_ROM_VERSION, NULL, 0, &p, &plen, HCI_INIT_TIMEOUT))
        return false;
    if (plen != 2 || p[0] != 0) {
        XYLog("read rom version failed (len %u status 0x%02x)\n", plen, plen ? p[0] : 0xff);
        return false;
    }
    *version = p[1];
    return true;
}

bool RTW89BTFirmwareCore::
readReg16(const RtlVendorCmd *cmd, uint8_t out[2])
{
    const uint8_t *p = NULL;
    uint32_t plen = 0;
    if (!hciCmd(RTL_OP_READ_REG16, cmd->param, sizeof(cmd->param), &p, &plen, HCI_INIT_TIMEOUT))
        return false;
    if (plen != 3 || p[0] != 0)
        return false;
    out[0] = p[1];
    out[1] = p[2];
    return true;
}

#define RTL_MAX_SUBSECS 512

OSData *RTW89BTFirmwareCore::
parseFirmware(uint8_t *fwData, uint32_t fwLen)
{
    if (!m_icInfo || !fwData || fwLen <= 8 || fwLen > RTW89BT::MaxFirmware)
        return NULL;

    bool isV2 = memcmp(fwData, RTL_EPATCH_SIGNATURE_V2, 8) == 0;
    bool isV1 = memcmp(fwData, RTL_EPATCH_SIGNATURE, 8) == 0;
    if (!isV1 && !isV2) {
        XYLog("bad epatch signature\n");
        return NULL;
    }

    int project_id = rtlFindProjectId(fwData, fwLen);
    if (project_id < 0) {
        XYLog("failed to find project-id instruction (%d)\n", project_id);
        return NULL;
    }
    uint16_t lmp = rtlLmpForProjectId(project_id);
    if (lmp == 0) {
        XYLog("unknown project id %d\n", project_id);
        return NULL;
    }
    if (project_id != m_icInfo->projectId || lmp != m_icInfo->lmpSubversion) {
        XYLog("firmware project %d, expected %u for %s\n", project_id,
              m_icInfo->projectId, m_icInfo->chipName);
        return NULL;
    }
    XYLog("firmware project id %d (lmp %04x, %s)\n", project_id, lmp, isV2 ? "v2" : "v1");

    uint8_t *out = (uint8_t *)IOMalloc(fwLen);
    if (!out)
        return NULL;
    OSData *result = NULL;
    uint32_t len = 0;

    if (isV2) {
        size_t scratchBytes = sizeof(RtlSubsecRef) * RTL_MAX_SUBSECS;
        RtlSubsecRef *scratch = (RtlSubsecRef *)IOMalloc(scratchBytes);
        if (scratch) {
            len = rtlParseFirmwareV2(fwData, fwLen, m_romVersion, m_keyId,
                                     scratch, RTL_MAX_SUBSECS, out, fwLen);
            IOFree(scratch, scratchBytes);
        }
    } else {
        len = rtlParseFirmwareV1(fwData, fwLen, m_romVersion, out, fwLen);
    }

    if (len > 0 && len <= fwLen) {
        XYLog("parsed firmware payload: %u bytes (rom_version %u key_id %u)\n",
              len, m_romVersion, m_keyId);
        result = OSData::withBytes(out, len);
    } else {
        XYLog("firmware parse produced no usable payload (%u)\n", len);
    }

    IOFree(out, fwLen);
    return result;
}

bool RTW89BTFirmwareCore::
downloadFirmware(const uint8_t *data, uint32_t fwLen)
{
    uint8_t command[RTW89BT::FragmentSize + 1];
    if (!data || !fwLen || fwLen > RTW89BT::MaxFirmware) return false;
    const size_t count = rtlFragmentCount(fwLen);
    for (size_t i = 0; i < count; ++i) {
        const size_t size = RTW89BT::fragment(data, fwLen, i, command, sizeof(command));
        const uint8_t *params = nullptr;
        uint32_t length = 0;
        if (!size || !hciCmd(RTL_OP_DOWNLOAD, command, uint8_t(size), &params, &length, HCI_CMD_TIMEOUT) ||
            length != 2) {
            XYLog("firmware fragment %u acknowledgement failed\n", unsigned(i));
            return false;
        }
    }
    HciRpReadLocalVersion ver;
    if (!readLocalVersion(&ver) || ver.manufacturer != 0x005d ||
        (ver.hci_rev == 0x000b && ver.lmp_subver == 0x8852)) {
        XYLog("firmware version did not change after upload\n");
        return false;
    }
    XYLog("post-download HCI revision %04x LMP subversion %04x\n", ver.hci_rev, ver.lmp_subver);
    return true;
}

OSData *RTW89BTFirmwareCore::requestFirmwareData(const char *name, bool noWarn)
{
    OSData *data = getFWDescByName(name);
    if (!data && !noWarn) XYLog("firmware %s unavailable\n", name);
    return data;
}

bool RTW89BTFirmwareCore::loadFirmware()
{
    if (m_icInfo->readRomVersion) {
        if (!readRomVersion(&m_romVersion))
            return false;
        XYLog("rom_version %u\n", m_romVersion);
    }

    m_keyId = 0;
    RtlVendorCmd secProj = { { 0x10, 0xA4, 0xAD, 0x00, 0xb0 } };
    uint8_t reg[2];
    if (readReg16(&secProj, reg))
        m_keyId = reg[0];
    else {
        XYLog("key-id read failed; refusing to choose security sections\n");
        return false;
    }

    OSData *fwRaw = requestFirmwareData(m_icInfo->filename);
    if (!fwRaw)
        return false;

    OSData *payload = parseFirmware((uint8_t *)fwRaw->getBytesNoCopy(), fwRaw->getLength());
    OSSafeReleaseNULL(fwRaw);
    if (!payload) {
        XYLog("firmware parse failed\n");
        return false;
    }

    const bool ok = downloadFirmware((const uint8_t *)payload->getBytesNoCopy(),
                                     payload->getLength());
    OSSafeReleaseNULL(payload);
    return ok;
}

bool RTW89BTFirmwareCore::setup()
{
    HciRpReadLocalVersion ver;
    if (!hciCmd(HCI_OP_RESET, nullptr, 0, nullptr, nullptr, HCI_CMD_TIMEOUT) ||
        !readLocalVersion(&ver))
        return false;
    XYLog("hci_ver=%02x hci_rev=%04x lmp_ver=%02x lmp_subver=%04x\n",
          ver.hci_ver, ver.hci_rev, ver.lmp_ver, ver.lmp_subver);

    m_icInfo = RTW89BT::findRunningFirmwareProfile(ver.manufacturer, ver.lmp_subver,
                                                   ver.hci_rev, ver.hci_ver);
    const bool alreadyRunning = m_icInfo != nullptr;
    if (!m_icInfo)
        m_icInfo = RTW89BT::findFirmwareProfile(ver.manufacturer, ver.lmp_subver, ver.hci_rev, ver.hci_ver);
    if (!m_icInfo) {
        XYLog("unsupported controller (lmp_subver %04x hci_rev %04x hci_ver %02x)\n",
              ver.lmp_subver, ver.hci_rev, ver.hci_ver);
        return false;
    }
    strlcpy(m_fwName, m_icInfo->chipName, sizeof(m_fwName));
    XYLog("matched %s\n", m_icInfo->chipName);
    if (!alreadyRunning && !loadFirmware())
        return false;

    readAdvertisingCapabilities(); // read-only, before releasing HCI USB ownership
    uint64_t deadline;
    clock_interval_to_deadline(HCI_CMD_TIMEOUT, kMillisecondScale, &deadline);
    // USB must be idle and the last command settled before the next
    // transport starts reading events or sending commands on this device.
    if (!m_pRTW89BTTransport->canHandOff() || !waitForCommandCredit(deadline)) {
        XYLog("HCI command channel unsettled; firmware handoff withheld\n");
        return false;
    }
    XYLog("firmware %s for %s\n", alreadyRunning ? "already running" : "download complete", m_icInfo->chipName);
    return true;
}
