#****************************************************************************************
# 在下下方添加您编译选项，包括编译器选项和编译需要的头文件															*
#****************************************************************************************
TUYA_PLATFORM_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# tuya os adapter includes
# RK3506B BLE: reference BlueZ stack (HCI + GATT via bluez gdbus) in tuyaos_adapter/src
TUYA_DBUS_SYSROOT := /home/swimming/share/RK3506B/atk_dlrk3506_linux6.1_release_v1.3.1_20260326/buildroot/output/alientek_rk3506/host/arm-buildroot-linux-gnueabihf/sysroot
TUYA_PLATFORM_CFLAGS := \
	-I$(TUYA_DBUS_SYSROOT)/usr/include \
	-I$(TUYA_DBUS_SYSROOT)/usr/include/dbus-1.0 \
	-I$(TUYA_DBUS_SYSROOT)/usr/lib/dbus-1.0/include \
	-I$(TUYA_DBUS_SYSROOT)/usr/include/glib-2.0 \
	-I$(TUYA_DBUS_SYSROOT)/usr/lib/glib-2.0/include \
	-I$(TUYA_PLATFORM_DIR)/tuyaos_adapter/src \
	-I$(TUYA_PLATFORM_DIR)/tuyaos_adapter/src/gdbus
