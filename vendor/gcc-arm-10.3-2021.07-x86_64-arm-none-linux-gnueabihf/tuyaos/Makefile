# --- RK3506B port: vendor relocated into atk/app/.../vendor/ and reached from
#     the TuyaOS tree via a symlink. The original self-locating paths below
#     resolved to the PHYSICAL atk dir through the symlink and broke the build:
#       - ROOT_DIR = $(abspath ../../../)  -> atk (no libs/)            => cannot find -ltuyaos
#       - -include ../../../build/build_param (skipped) + inherited CXX lost => host g++ link
#     So hardcode the TuyaOS root and the armhf cross-toolchain here.
TUYAOS_ROOT := /home/swimming/share/RK3506B/atk_dlrk3506_linux6.1_release_v1.3.1_20260326/app/tuyaos_demo_wukong_ai
TUYA_TC_BIN := $(TUYAOS_ROOT)/vendor/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/toolchain/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin
ROOT_DIR ?= $(TUYAOS_ROOT)
-include $(TUYAOS_ROOT)/build/build_param
CC  := $(TUYA_TC_BIN)/arm-none-linux-gnueabihf-gcc
CXX := $(TUYA_TC_BIN)/arm-none-linux-gnueabihf-g++

#编译所用变量
OUTPUT_DIR = $(ROOT_DIR)/$(APP_PATH)/output/$(APP_NAME)_$(APP_VER)

#库文件路径
TUYAOS_LIB_DIR = $(ROOT_DIR)/libs

#链接选项
# ifeq ($(PLAT_DEBUG_WITH_ASAN))
# LINKFLAGS = -L$(TUYAOS_LIB_DIR) -Xlinker "-(" -l$(APP_NAME) -ltuyaapp_components -ltuyaapp_drivers -ltuyaos -ltuyaos_adapter -Xlinker "-)" -fsanitize=address -pthread -lm -std=c++11
# else
# BLE (reference BlueZ): link against buildroot sysroot (dbus/glib/bluetooth)
TUYA_SYSROOT := /home/swimming/share/RK3506B/atk_dlrk3506_linux6.1_release_v1.3.1_20260326/buildroot/output/alientek_rk3506/host/arm-buildroot-linux-gnueabihf/sysroot
LINKFLAGS = --sysroot=$(TUYA_SYSROOT) -L$(TUYAOS_LIB_DIR) -Xlinker "-(" -l$(APP_NAME) -ltuyaapp_components -ltuyaapp_drivers -ltuyaos -ltuyaos_adapter -Xlinker "-)" -L$(TUYA_SYSROOT)/usr/lib -Wl,-rpath-link,$(TUYA_SYSROOT)/usr/lib -Wl,-rpath-link,$(TUYA_SYSROOT)/lib -ldbus-1 -lglib-2.0 -lgobject-2.0 -lgio-2.0 -lbluetooth -pthread -lm -std=c++11
# endif


all: app_excute
app_excute:
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(LINKFLAGS) -o $(OUTPUT_DIR)/$(APP_NAME)
	@echo "Build APP Finish"

.PHONY: all clean SHOWARGS app_excute pack
clean:
	rm -rf $(OUTPUT_DIR)
