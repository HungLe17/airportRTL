AIRPORT_BT := airportRTLBluetooth
AIRPORT_BT_ID := io.github.HungLe17.airportRTLBluetooth
AIRPORT_BT_SRC := bluetooth
AIRPORT_BT_CONTENTS := $(BUILD_DIR)/out/$(AIRPORT_BT).kext/Contents
AIRPORT_BT_OBJECTS := $(BT_OBJS) $(BUILD_DIR)/bluetooth/module.o
AIRPORT_BT_TEST := $(BUILD_DIR)/tests/advertising
ADVERTISING_BUILD_ID := 2654ad3
ADVERTISING_BUILD := $(abspath $(BUILD_DIR))/bluetooth/broadtek
ADVERTISING_OBJECTS := $(addprefix $(ADVERTISING_BUILD)/obj/,bt_aes.o bt_hci.o bt_smp.o \
	kern_broadtek.o kern_usbclass.o kern_start.o plugin_start.o)

.PHONY: airportRTL-bluetooth airportRTL-tests airportRTL-bluetooth-advertising
.DELETE_ON_ERROR:

airportRTL-tests: $(AIRPORT_BT_TEST)
	python3 tools/test_bluetooth.py --sanitize
	$(AIRPORT_BT_TEST)

$(AIRPORT_BT_TEST): tests/advertising.cpp $(AIRPORT_BT_SRC)/AdvertisingSets.hpp $(BT_SRC)/BtProtocol.hpp
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $< -o $@

$(BUILD_DIR)/bluetooth/module.o: module.c $(AIRPORT_BT_SRC)/rules.mk Makefile
	@mkdir -p $(dir $@)
	$(CC) $(FW_CFLAGS) -DMODULE_ID=$(AIRPORT_BT_ID) -DMODULE_VERSION='"$(AIRPORTRTL_VERSION)"' \
		-DMODULE_START=BroadTek_kern_start -DMODULE_STOP=BroadTek_kern_stop -c $< -o $@

$(AIRPORT_BT_CONTENTS)/Info.plist: $(BT_SRC)/Info.plist $(ADVERTISING_SOURCE)/src/plugin/Info.plist $(AIRPORT_BT_SRC)/bundle_info.py $(AIRPORT_BT_SRC)/rules.mk Makefile
	@mkdir -p $(dir $@)
	python3 $(AIRPORT_BT_SRC)/bundle_info.py "$(word 1,$^)" "$(word 2,$^)" "$@" $(AIRPORT_BT) $(AIRPORT_BT_ID) $(AIRPORTRTL_VERSION)

airportRTL-bluetooth-advertising:
	@test -f "$(LILU_KEXT)/Contents/MacOS/Lilu" || { echo "Set LILU_KEXT to the target Lilu.kext"; exit 1; }
	$(MAKE) -C "$(ADVERTISING_SOURCE)" BUILD="$(ADVERTISING_BUILD)" BUILD_ID=$(ADVERTISING_BUILD_ID) DEBUG=0 test $(ADVERTISING_OBJECTS)

$(AIRPORT_BT_CONTENTS)/MacOS/$(AIRPORT_BT): $(AIRPORT_BT_OBJECTS) airportRTL-bluetooth-advertising
	@mkdir -p $(dir $@)
	$(CXX) $(ARCH) $(MINOS) -isysroot $(SDK) -nostdlib -Xlinker -kext \
		-L$(MKSDK)/Library/x86_64 $(AIRPORT_BT_OBJECTS) $(ADVERTISING_OBJECTS) -lkmod -lcc_kext -o $@

airportRTL-bluetooth: airportRTL-tests $(AIRPORT_BT_CONTENTS)/Info.plist $(AIRPORT_BT_CONTENTS)/MacOS/$(AIRPORT_BT)
	@mkdir -p $(AIRPORT_BT_CONTENTS)/Resources
	cp $(BT_SRC)/SOURCE.md $(BT_SRC)/COPYING $(BT_SRC)/COPYING.GPL-3.0 firmware/rtl_bt/LICENCE.rtlwifi_firmware.txt $(AIRPORT_BT_CONTENTS)/Resources/
	cp $(AIRPORT_BT_SRC)/BroadTek-LICENSE.txt "$(ADVERTISING_SOURCE)/third_party/Lilu/LICENSE.txt" $(AIRPORT_BT_CONTENTS)/Resources/
	plutil -lint $(AIRPORT_BT_CONTENTS)/Info.plist
	codesign --force --sign - $(AIRPORT_BT_CONTENTS)/..
	codesign --verify --deep --strict $(AIRPORT_BT_CONTENTS)/..
	python3 tools/check_kpi.py $(AIRPORT_BT_CONTENTS)/.. --library-kext "$(LILU_KEXT)" --require-vtable-exports

-include $(AIRPORT_BT_OBJECTS:.o=.d)
