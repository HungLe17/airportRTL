# Included by the pinned reference Makefile for the integrated Wi-Fi kext.
ifneq ($(OWLY_ABI6_FRONT_BRIDGE),1)
$(error The integrated front requires OWLY_ABI6_FRONT_BRIDGE=1)
endif

WIFI_ID := io.github.HungLe17.airportRTLWiFi
WIFI_OBJECTS := $(filter-out $(BUILD_DIR)/kext/kmod_info.o,$(KEXT_OBJS))
WIFI_FRONT_FLAGS := $(KEXT_CXXFLAGS) -Wno-inconsistent-missing-override \
	-DRTW89_AWDL_NATIVE_FRONT_OBSERVE=$(RTW89_AWDL_NATIVE_FRONT_OBSERVE) \
	-DOWLY_GATE14_REAL_VIF=$(OWLY_GATE14_REAL_VIF) \
	-DOWLY_ABI6_NATIVE_P2P_FACTORY=$(OWLY_ABI6_NATIVE_P2P_FACTORY)

AIRPORT_BRIDGE_OBJ := $(BUILD_DIR)/integrated-bridge
AIRPORT_BRIDGE_COMMON := $(addprefix $(AIRPORT_BRIDGE_OBJ)/,owly_event_core.o Bootstrap.o Service.o Status.o plugin_start.o)
AIRPORT_FAMILY_UUID = $(shell xcrun dwarfdump --uuid "$(FAMILY_BIN)" | awk '$$3 == "(x86_64)" { print $$2 }')

$(AIRPORT_BRIDGE_OBJ)/OwlyTargets.hpp: $(BRIDGE_DIR)/src/OwlyTargets.hpp $(AIRPORT_SRC)/wifi/bridge-target.patch
	@mkdir -p $(dir $@)
	patch -s -o "$@.tmp" "$<" "$(word 2,$^)"
	mv "$@.tmp" "$@"

$(AIRPORT_BRIDGE_OBJ)/%.o: $(BRIDGE_DIR)/src/%.cpp $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CXX) $(BRIDGE_COMMON) $(BRIDGE_CXX) -MMD -MP -c "$<" -o "$@"

$(AIRPORT_BRIDGE_OBJ)/%.o: $(BRIDGE_DIR)/src/%.c $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CC) $(BRIDGE_COMMON) -std=gnu11 -MMD -MP -c "$<" -o "$@"

$(AIRPORT_BRIDGE_OBJ)/plugin_start.o: $(BRIDGE_DIR)/vendor/Lilu/Library/plugin_start.cpp $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CXX) $(BRIDGE_COMMON) $(BRIDGE_CXX) -MMD -MP -c "$<" -o "$@"

.PHONY: airportRTL-wifi-with-airlink
WIFI_AIRLINK_VERSION := 0.1.1
WIFI_AIRLINK_DESCRIPTOR := $(BUILD_DIR)/airport_airlink_module.o
WIFI_AIRLINK_BINARY := $(BUILD_DIR)/airportRTLWiFi.airlink
WIFI_AIRLINK_IOKIT := $(BUILD_DIR)/airlink-sdk
WIFI_AIRLINK_IOKIT_HEADER := $(WIFI_AIRLINK_IOKIT)/IOKit/network/IONetworkController.h
WIFI_AIRLINK_NETWORK_OBJECTS := $(addprefix $(BUILD_DIR)/airlink-network/,AirPortRTW89.o AirPortRTW89Native.o AirPortRTW89Front.o)
WIFI_AIRLINK_CORE := $(filter-out $(BUILD_DIR)/kext/AirPortRTW89.o $(BUILD_DIR)/kext/AirPortRTW89Native.o,$(ALL_OBJS) $(WIFI_OBJECTS)) $(WIFI_AIRLINK_NETWORK_OBJECTS)

$(BUILD_DIR)/overlay/AirPortRTW89Native.cpp: $(KEXT_SRC)/AirPortRTW89Native.cpp $(AIRPORT_SRC)/wifi/front-attach.patch
	@mkdir -p "$(@D)"
	patch -s -o "$@.tmp" "$<" "$(word 2,$^)"
	mv "$@.tmp" "$@"

$(WIFI_AIRLINK_IOKIT_HEADER): $(MKSDK)/Headers/IOKit/network/IONetworkController.h $(AIRPORT_SRC)/wifi/tahoe/IONetworkController.patch
	@mkdir -p $(dir $@)
	patch -s -o "$@.tmp" "$<" "$(word 2,$^)"
	mv "$@.tmp" "$@"

$(BUILD_DIR)/airlink-network/AirPortRTW89.o: $(KEXT_SRC)/AirPortRTW89.cpp $(WIFI_AIRLINK_IOKIT_HEADER) $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CXX) -I$(WIFI_AIRLINK_IOKIT) $(KEXT_CXXFLAGS) $(LOGRING_DEF) -c "$<" -o "$@"

$(BUILD_DIR)/airlink-network/AirPortRTW89Native.o: $(BUILD_DIR)/overlay/AirPortRTW89Native.cpp $(WIFI_AIRLINK_IOKIT_HEADER) $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CXX) -I$(WIFI_AIRLINK_IOKIT) $(KEXT_CXXFLAGS) $(LOGRING_DEF) -iquote $(COMPAT_DIR) -I$(KEXT_SRC) -c "$<" -o "$@"

$(BUILD_DIR)/airlink-network/AirPortRTW89Front.o: $(FRONT_SRC)/AirPortRTW89Front.cpp $(WIFI_AIRLINK_IOKIT_HEADER) $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CXX) -I$(WIFI_AIRLINK_IOKIT) $(WIFI_FRONT_FLAGS) -c "$<" -o "$@"

$(AIRPORT_BRIDGE_OBJ)/PluginIntegrated.cpp: $(BRIDGE_DIR)/src/Plugin.cpp $(AIRPORT_SRC)/wifi/bridge-integration.patch
	@mkdir -p $(dir $@)
	patch -s -o "$@.tmp" "$<" "$(word 2,$^)"
	mv "$@.tmp" "$@"

$(AIRPORT_BRIDGE_OBJ)/PluginIntegrated.o: $(AIRPORT_BRIDGE_OBJ)/PluginIntegrated.cpp $(AIRPORT_BRIDGE_OBJ)/OwlyTargets.hpp $(AIRPORT_SRC)/wifi/rules.mk
	$(CXX) $(BRIDGE_COMMON) $(BRIDGE_CXX) -MMD -MP -c "$<" -o "$@"

$(WIFI_AIRLINK_DESCRIPTOR): $(AIRPORT_SRC)/module.c $(AIRPORT_SRC)/wifi/rules.mk
	@mkdir -p $(dir $@)
	$(CC) $(FW_CFLAGS) -DMODULE_ID=$(WIFI_ID) -DMODULE_VERSION='"$(WIFI_AIRLINK_VERSION)"' \
	    -DMODULE_START=OwlyAirLinkBridge_kern_start -DMODULE_STOP=OwlyAirLinkBridge_kern_stop -c "$<" -o "$@"

$(WIFI_AIRLINK_BINARY): $(WIFI_AIRLINK_CORE) $(WIFI_AIRLINK_DESCRIPTOR) $(AIRPORT_BRIDGE_COMMON) $(AIRPORT_BRIDGE_OBJ)/PluginIntegrated.o
	$(CXX) $(ARCH) $(MINOS) -isysroot $(SDK) -nostdlib -Xlinker -kext -L$(MKSDK)/Library/x86_64 \
	    $^ -lkmod -lcc_kext -o "$@"

airportRTL-wifi-with-airlink: $(WIFI_AIRLINK_BINARY)
	python3 tools/bind_bridge.py --check "$(AIRPORT_FAMILY_UUID)" "$(WIFI_AIRLINK_BINARY)"
	python3 "$(AIRPORT_SRC)/wifi/check_layout.py" src "$(WIFI_AIRLINK_BINARY)" "$(AIRPORT_FAMILY)" $(WIFI_ID) $(WIFI_AIRLINK_VERSION) \
	    --bridge "$(BRIDGE_DIR)/Info.plist" --lilu "$(LILU_KEXT)" --output "$(BUILD_DIR)/../integrated"

-include $(AIRPORT_BRIDGE_COMMON:.o=.d) $(AIRPORT_BRIDGE_OBJ)/PluginIntegrated.d $(WIFI_AIRLINK_NETWORK_OBJECTS:.o=.d)
