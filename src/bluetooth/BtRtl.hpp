//
//  RTW89BTFirmwareCore.hpp
//  RealtekBluetoothFirmware
//
//  Created by theVakhovskeIsTaken on 2026/8/9.
//  Copyright © 2026 thegwchr. All rights reserved.
//

#ifndef RTW89BTFirmwareCore_hpp
#define RTW89BTFirmwareCore_hpp

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSData.h>
#include <libkern/libkern.h>

#include "USBDeviceController.hpp"
#include "Hci.h"
#include "BtProtocol.hpp"

#define RTL_FRAG_LEN            252

#define RTL_EPATCH_SIGNATURE    "Realtech"
#define RTL_EPATCH_SIGNATURE_V2 "RTBTCore"

#define RTL_ROM_LMP_8723A       0x1200
#define RTL_ROM_LMP_8723B       0x8723
#define RTL_ROM_LMP_8821A       0x8821
#define RTL_ROM_LMP_8761A       0x8761
#define RTL_ROM_LMP_8703B       0x8703
#define RTL_ROM_LMP_8822B       0x8822
#define RTL_ROM_LMP_8852A       0x8852
#define RTL_ROM_LMP_8851B       0x8851
#define RTL_ROM_LMP_8922A       0x8922

#define RTL_OP_READ_ROM_VERSION 0xfc6d
#define RTL_OP_READ_REG16       0xfc61
#define RTL_OP_DOWNLOAD         0xfc20

#define RTL_PATCH_SNIPPETS         0x01
#define RTL_PATCH_DUMMY_HEADER     0x02
#define RTL_PATCH_SECURITY_HEADER  0x03

typedef struct __attribute__((packed)) {
    uint8_t  index;
    uint8_t  data[RTL_FRAG_LEN];
} RtlDownloadCmd;

typedef struct __attribute__((packed)) {
    uint8_t  param[5];
} RtlVendorCmd;

class RTW89BTFirmwareCore : public OSObject {
    OSDeclareDefaultStructors(RTW89BTFirmwareCore)

public:
    bool initWithDevice(IOService *client, IOUSBHostDevice *dev);
    void free() override;
    bool setup();
    uint8_t releaseUSBForHandoff();
    bool getFirmwareName(char *fwname, size_t len);
    const RTW89BT::AdvertisingCapabilities &advertisingCapabilities() const { return m_advertising; }

private:
    bool hciCmd(uint16_t opcode, const void *param, uint8_t plen,
                const uint8_t **outParams, uint32_t *outParamLen, int timeout);
    IOReturn readHCIEvent(uint32_t &size, uint64_t deadline);
    bool waitForCommandCredit(uint64_t deadline);

    bool readLocalVersion(HciRpReadLocalVersion *out);
    bool readRomVersion(uint8_t *version);
    bool readReg16(const RtlVendorCmd *cmd, uint8_t out[2]);
    void readAdvertisingCapabilities();


    OSData *parseFirmware(uint8_t *fwData, uint32_t fwLen);
    bool loadFirmware();
    bool downloadFirmware(const uint8_t *data, uint32_t fwLen);

    OSData *requestFirmwareData(const char *fwName, bool noWarn = false);

    RTW89BTTransport *m_pRTW89BTTransport = nullptr;
    const RTW89BT::FirmwareProfile *m_icInfo;
    uint8_t              m_romVersion;
    uint8_t              m_keyId;
    char                 m_fwName[48];

    uint8_t              m_cmdBuf[260];
    uint8_t              m_evtBuf[260];
    RTW89BT::CommandFlow m_hciFlow;
    RTW89BT::AdvertisingCapabilities m_advertising;
};

#endif /* RTW89BTFirmwareCore_hpp */
