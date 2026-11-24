.DEFAULT_GOAL := package
.DELETE_ON_ERROR:

AIRPORTRTL_VERSION := 0.1.2
BUILD_DIR ?= build/airportRTL
WIFI_BUILD_DIR := $(abspath $(BUILD_DIR))/wifi-link
MKSDK := ./third_party/MacKernelSDK
COMPAT_DIR := ./src/compat
FIRMWARE_DIR := ./firmware

REFERENCE_WIFI := third_party/AirPort_RTW89
ADVERTISING_SOURCE := $(abspath third_party/BroadTek)
FAMILY_KEXT ?= build/deps/IO80211FamilyLegacy.kext
LILU_KEXT ?= build/deps/Lilu.kext

SDK          := $(shell xcrun --show-sdk-path 2>/dev/null)
ARCH         := -arch x86_64
MINOS        := -mmacosx-version-min=13.0
CC           := xcrun clang
CXX          := xcrun clang++

KEXT_FLAGS   := -fno-exceptions -fno-stack-protector -mkernel \
                -O2 -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks \
                -fno-omit-frame-pointer -Wframe-larger-than=1024 \
                -MMD -MP \
                $(ARCH) $(MINOS) \
                -isysroot $(SDK) \
                -I$(MKSDK)/Headers \
                -I$(SDK)/System/Library/Frameworks/Kernel.framework/Headers

LOGRING_DEF := -DIOLog=rtw89_iolog
FW_CFLAGS := $(KEXT_FLAGS) $(LOGRING_DEF) -std=gnu11 -DKERNEL -I$(COMPAT_DIR)

KEXT_CXXFLAGS := $(ARCH) $(MINOS) -isysroot $(SDK) -nostdinc \
                 -std=gnu++17 -O2 -fno-strict-aliasing -mkernel -fapple-kext -fno-rtti \
                 -fno-exceptions \
                 -fno-builtin -fno-common -fno-stack-protector \
                 -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
                 -D__PRIVATE_SPI__ -D__IO80211_TARGET=__MAC_13_0 \
                 -I$(MKSDK)/Headers -Wall -MMD -MP

BT_SRC := ./src/bluetooth
BT_FIRMWARE := ./firmware/rtl_bt/rtl8852bu_fw.bin
BT_OBJS := $(addprefix $(BUILD_DIR)/bluetooth/,RTW89BluetoothFirmware.o BtRtl.o USBDeviceController.o bt_firmware.o)

$(BUILD_DIR)/gen/bt_firmware.cpp: $(BT_FIRMWARE) tools/embed_bt_firmware.py
	@python3 tools/embed_bt_firmware.py $< $@

$(BUILD_DIR)/bluetooth/%.o: $(BT_SRC)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(KEXT_CXXFLAGS) -Wframe-larger-than=1024 -I$(BT_SRC) -c $< -o $@

$(BUILD_DIR)/bluetooth/bt_firmware.o: $(BUILD_DIR)/gen/bt_firmware.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(KEXT_CXXFLAGS) -c $< -o $@

include bluetooth/rules.mk

.PHONY: package wifi bluetooth test check-wifi-firmware

package: wifi bluetooth
	python3 package.py "$(abspath $(BUILD_DIR))" "$(AIRPORTRTL_VERSION)"

bluetooth: airportRTL-bluetooth

test: airportRTL-tests
	python3 tests/layout_test.py

check-wifi-firmware:
	@cd "$(FIRMWARE_DIR)" && shasum -a 256 -c SHA256SUMS

wifi: check-wifi-firmware
	@test -f "$(FAMILY_KEXT)/Contents/MacOS/IO80211FamilyLegacy" || { echo "Set FAMILY_KEXT to the target IO80211FamilyLegacy.kext"; exit 1; }
	@test -f "$(LILU_KEXT)/Contents/MacOS/Lilu" || { echo "Set LILU_KEXT to the target Lilu.kext"; exit 1; }
	$(MAKE) -C "$(REFERENCE_WIFI)" -f Makefile -f "$(CURDIR)/wifi/rules.mk" \
	    AIRPORT_SRC="$(CURDIR)" AIRPORT_FAMILY="$(abspath $(FAMILY_KEXT))" \
	    AIRPORTRTL_VERSION="$(AIRPORTRTL_VERSION)" \
	    BUILD_DIR="$(WIFI_BUILD_DIR)" COMPAT_DIR="$(abspath $(COMPAT_DIR))" \
	    LINUX_INC="$(abspath third_party/linux-include)" FIRMWARE_DIR="$(abspath $(FIRMWARE_DIR))" \
	    FAMILY_BIN="$(abspath $(FAMILY_KEXT))/Contents/MacOS/IO80211FamilyLegacy" \
	    LILU_KEXT="$(abspath $(LILU_KEXT))" airportRTL-wifi-with-airlink
