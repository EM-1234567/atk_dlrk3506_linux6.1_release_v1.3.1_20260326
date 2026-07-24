############################################################
# 应用层单元测试编排（独立于 scripts/ut/Makefile）
#
# 与 scripts/ut/Makefile 的区别：
#   - 仅扫描 application_components/、application_drivers/、apps/ 下的 ut.mk
#     （不含 components/ 的 SDK 单元测试）
#   - target 名为 app_ut / app_ut_run / app_ut_files / app_ut_pack
#   - 产物输出到 output/ut_app/（与 output/ut/ 隔离）
#
# 入口：仓库根目录执行 `make app_ut`（由 scripts/mk/ut.mk 转发）
############################################################

############################################################
# 全局环境变量配置
############################################################
TOPDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../)
-include $(TOPDIR)/build/tuya_iot.config
OUTPUT_DIR ?= $(TOPDIR)/output/ut_app

CFLAGS   := -g -Werror-implicit-function-declaration
CXXFLAGS := -g --std=c++11

COV ?= 1
LOG ?= 1
LCOV_REMOVE_PATTERNS ?= \
	'*/ut/test/*' \
	'*/scripts/ut/externals/gtest/*' \
	'*/scripts/ut/externals/mockcpp/*' \
	'/usr/include/*'

# gcov覆盖率检测
ifeq ($(COV), 1)
CFLAGS   += --coverage
CXXFLAGS += --coverage
XMAKE_EXECUTABLE_LDFLAGS += --coverage
endif

############################################################
# 默认编译目标
############################################################
all: app_ut_run
ifeq ($(COV), 1)
all: coverage
endif


############################################################
# 使用 xmake
############################################################
XMAKE_OUTDIR := $(OUTPUT_DIR)
_XMAKE_TOPDIR := $(TOPDIR)
-include $(TOPDIR)/scripts/mk/xmake.mk
TUYA_UT := 1
CFLAGS += -DTUYA_UT=$(TUYA_UT)
CXXFLAGS += -DTUYA_UT=$(TUYA_UT)
ifeq ($(TARGET_PLATFORM),)
TARGET_PLATFORM := $(notdir $(firstword $(wildcard $(TOPDIR)/vendor/*)))
endif
UT_VENDOR_INC_ALL_SUBDIRS := $(shell find $(TOPDIR)/vendor/$(TARGET_PLATFORM)/tuyaos/tuyaos_adapter/include -type d 2>/dev/null)
CFLAGS += $(addprefix -I,$(UT_VENDOR_INC_ALL_SUBDIRS))
CXXFLAGS += $(addprefix -I,$(UT_VENDOR_INC_ALL_SUBDIRS))


############################################################
# 包含所有SDK组件的local.mk 并 配置头文件路径
############################################################
TUYA_SDK_INC :=				 # 加载至CFLAGS中提供给其他组件使用；打包进SDK产物中；
TUYA_SDK_CFLAGS :=			 # 模块对外CFLAGS：其他组件编译时可感知到
-include $(shell find $(TOPDIR)/adapter $(TOPDIR)/components -name local.mk 2>/dev/null)
TUYA_SDK_INC_ALL_SUBDIRS := $(foreach dir,$(TUYA_SDK_INC),$(shell find $(dir) -type d))
TUYA_SDK_INC_ALL_SUBDIRS += $(TOPDIR)/include/base/include
TUYA_SDK_INC_ALL_SUBDIRS += $(shell find $(TOPDIR)/include/components -type d 2>/dev/null)
CFLAGS += $(addprefix -I,$(TUYA_SDK_INC_ALL_SUBDIRS))
CFLAGS += $(TUYA_SDK_CFLAGS)
CXXFLAGS += $(addprefix -I,$(TUYA_SDK_INC_ALL_SUBDIRS))


############################################################
# 仅扫描应用层目录的 ut.mk（与 scripts/ut/Makefile 的关键差异）
############################################################
UT_CLEAR_VARS   := $(TOPDIR)/scripts/ut_app/ut_clear.mk
UT_BUILD_TARGET := $(TOPDIR)/scripts/ut_app/ut_target.mk
UT_TARGETS      :=
TUYA_UT_INC :=
# 注意：本 Makefile 通过 scripts/mk/ut.mk 用 `cd scripts/ut && make -f ../ut_app/Makefile`
# 方式调用，cwd 落在 scripts/ut/ 下，ut_target.mk 内的相对路径 `externals/gtest/lib`
# 才能正确解析。本地不要再次覆盖 LOCAL_LDFLAGS。
-include $(shell find $(TOPDIR)/application_components $(TOPDIR)/application_drivers $(TOPDIR)/apps -name ut.mk 2>/dev/null)
UT_INC_ALL_SUBDIRS := $(foreach dir,$(TUYA_UT_INC),$(shell find $(dir) -type d))
CXXFLAGS += $(addprefix -I,$(UT_INC_ALL_SUBDIRS))


############################################################
# app_ut 目标定义
############################################################
# gtest/mockcpp 的绝对路径（避免 -C scripts/ut_app 后的相对路径解析问题）
GTEST_LIB_DIR := $(TOPDIR)/scripts/ut/externals/gtest/lib
MOCKCPP_LIB_DIR := $(TOPDIR)/scripts/ut/externals/mockcpp/lib
# 判断是否使用 gdb
UT_USE_GDB :=
ifeq ($(GDB),1)
ifneq ($(TEST),)
UT_USE_GDB := 1
endif
endif

# 参数判断
UT_ARG_FILTER :=
UT_ARG_REPEAT :=
UT_ARG_RUN_DISABLED :=
UT_ARG_PRINT_TIME :=
UT_ARG_OUTPUT :=
ifneq ($(FILTER),)
UT_ARG_FILTER := "--gtest_filter=$(FILTER)"
endif
ifneq ($(REPEAT),)
UT_ARG_FILTER := "--gtest_repeat=$(REPEAT)"
endif
ifeq ($(RUN_DISABLED),1)
UT_ARG_RUN_DISABLED := "--gtest_also_run_disabled_tests"
endif
ifeq ($(PRINT_TIME),0)
UT_ARG_PRINT_TIME := "--gtest_print_time=0"
endif
ifeq ($(LOG),1)
UT_ARG_OUTPUT := "--gtest_output=json:$(XMAKE_OUTDIR)/log/"
endif

# 单元测试运行规则定义
app_ut_run: prepare
app_ut_run/%: $(XMAKE_OUTDIR)/bin/ut_%
	@echo "-------- run `basename $^` unit test ------------------------"
ifeq ($(UT_USE_GDB),1)
	cd $(dir $^); gdb $(notdir $^)
else
	cd $(dir $^); ./$(notdir $^) $(UT_ARG_FILTER) $(UT_ARG_REPEAT) $(UT_ARG_RUN_DISABLED) $(UT_ARG_PRINT_TIME) $(UT_ARG_OUTPUT)
endif

ifeq ($(TEST),)
app_ut: $(UT_TARGETS)
app_ut_run: $(patsubst $(XMAKE_OUTDIR)/bin/ut_%,app_ut_run/%,$(UT_TARGETS))
else
app_ut: $(XMAKE_OUTDIR)/bin/ut_$(TEST)
app_ut_run: app_ut_run/$(TEST)
endif

############################################################
# 获取覆盖率
############################################################
coverage:
	make gcov
	make lcov

gcov:
	@_gcda=`find $(OUTPUT_DIR) -name "*.gcda"`; \
	_c=`echo $$_gcda | sed 's,\.gcda,\.c,g'`; echo $$_c; \
	_src=`echo $$_c | grep -oP "(?<=/)\w*\.\w*(?=\.c)"`; echo $$_src; \
	_gcov=`echo $$_src.gcov | sed 's, ,\.gcov ,g'`; echo $$_gcov; \
	gcov $$_c; \
	mkdir $(OUTPUT_DIR)/gcov; \
	cp -f $$_gcov -t $(OUTPUT_DIR)/gcov; \
	rm *.gcov

lcov:
	lcov -c -o $(OUTPUT_DIR)/locv.raw.info -d $(OUTPUT_DIR)
	lcov --remove $(OUTPUT_DIR)/locv.raw.info $(LCOV_REMOVE_PATTERNS) -o $(OUTPUT_DIR)/locv.info
	genhtml $(OUTPUT_DIR)/locv.info -o $(OUTPUT_DIR)/lcov
	mv $(OUTPUT_DIR)/locv.info $(OUTPUT_DIR)/lcov
	rm -f $(OUTPUT_DIR)/locv.raw.info


############################################################
# app_ut 打包
############################################################
ifeq ($(UT_FILES_PATH_NAME),)
UT_FILES_PATH_NAME=ut_app_log
endif
UT_FILES_DIR=$(dir $(OUTPUT_DIR))/$(UT_FILES_PATH_NAME)
app_ut_files:
	@echo "====== build app ut files begin ======="
	# 清空目录
	@rm -fr $(UT_FILES_DIR)
	@mkdir -p $(UT_FILES_DIR)
	@cp -rf $(OUTPUT_DIR)/log -t $(UT_FILES_DIR)
	@cp -rf $(OUTPUT_DIR)/lcov -t $(UT_FILES_DIR)
	@cp -rf $(OUTPUT_DIR)/gcov -t $(UT_FILES_DIR)
	@echo "------ build app ut files end ---------"

ifeq ($(UT_PACKAGE_FULLNAME),)
UT_PACKAGE_FULLNAME := $(dir $(OUTPUT_DIR))/dist/$(UT_FILES_PATH_NAME).tar.gz
endif
app_ut_pack: app_ut_files
	@echo "====== app ut pack begin ======="
	@rm -fr $(UT_FILES_DIR)/../ut.tar.gz
	@cd $(UT_FILES_DIR)/../ && tar zcvf ut.tar.gz $(UT_FILES_PATH_NAME)
	@rm -rf $(UT_PACKAGE_FULLNAME)
	@mkdir -p `dirname $(UT_PACKAGE_FULLNAME)`
	@mv $(UT_FILES_DIR)/../ut.tar.gz $(UT_PACKAGE_FULLNAME)
	@echo "------ app ut pack end ---------"

############################################################
# 外部依赖：与 scripts/ut/Makefile 共享同一份 gtest/mockcpp 克隆
############################################################
prepare: prepare_gtest

prepare_gtest: $(TOPDIR)/scripts/ut/externals/gtest/lib/libgtest.a

$(TOPDIR)/scripts/ut/externals/gtest/lib/libgtest.a:
	@rm -fr $(TOPDIR)/scripts/ut/externals
	@git clone https://registry.code.tuya-inc.top/embed_ci_space_group/gtest.git $(TOPDIR)/scripts/ut/externals/gtest
	@git clone https://registry.code.tuya-inc.top/embed_ci_space_group/mockcpp.git $(TOPDIR)/scripts/ut/externals/mockcpp

############################################################
# 其他目标和依赖关系
############################################################
.PHONY: all clean app_ut app_ut_run app_ut_files app_ut_pack prepare prepare_gtest

# 清空中间文件
clean:
	rm -fr $(OUTPUT_DIR)
