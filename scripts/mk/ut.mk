############################################################
# 执行单元测试
############################################################
PRJ_LIB := $(ROOT_DIR)/libs/libtuyaos.a $(ROOT_DIR)/libs/libtuyaos_adapter.a
PRJ_INC := $(addprefix $(ROOT_DIR)/,$(TUYA_SDK_INC_ALL_SUBDIRS))
OUTPUT_UT_DIR := $(OUTPUT_DIR)/ut
ut:
	@make -C scripts/ut all PRJ_LIB="$(PRJ_LIB)" PRJ_INC="$(PRJ_INC)" OUTPUT_DIR="$(OUTPUT_UT_DIR)"

clean_ut:
	@rm -rf $(OUTPUT_UT_DIR)

clean: clean_ut

.PHONY: ut clean_ut
ut: os

############################################################
# 应用层单元测试（独立编排：仅 application_components / application_drivers / apps）
# 与 `make ut` 共存，但产物隔离到 output/ut_app/，且不包含 components/ 的 SDK UT
############################################################
app_ut:
	@cd scripts/ut && $(MAKE) -f ../ut_app/Makefile all PRJ_LIB="$(PRJ_LIB)" PRJ_INC="$(PRJ_INC)" OUTPUT_DIR="$(OUTPUT_DIR)/ut_app"

app_ut_clean:
	@rm -rf $(OUTPUT_DIR)/ut_app

.PHONY: app_ut app_ut_clean
app_ut: os
