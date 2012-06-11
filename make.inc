##
## kernel/linux/make.inc
##
## History:
##    2012/06/01 - [Cao Rongrong] Created file
##
## Copyright (C) 2012-2016, Ambarella, Inc.
##
## All rights reserved. No Part of this file may be reproduced, stored
## in a retrieval system, or transmitted, in any form, or by any means,
## electronic, mechanical, photocopying, recording, or otherwise,
## without the prior consent of Ambarella, Inc.
##

ARCH := arm
export ARCH

LOCAL_PATH:=$(call my-dir)

.PHONY: linux

linux:
	@mkdir -p $(LINUX_OUT_DIR)
	@echo -n "Build Linux "
	@if [ -f $(LINUX_OUT_DIR)/.config ]; then \
		echo "with previous configuration ..."; \
	else \
		echo "with ambarella_$(KERNEL_DEFCONFIG)_defconfig ..."; \
		$(MAKE) $(AMBA_MAKE_PARA) $(LINUX_MAKE_FLAG) ambarella_$(KERNEL_DEFCONFIG)_defconfig; \
	fi
	$(AMBA_MAKEFILE_V)$(MAKE) $(AMBA_MAKE_PARA) $(LINUX_MAKE_FLAG) all
	@if [ -n "$(KERNEL_INSTALL_PATH)" ]; then \
		echo "Install Linux modules to $(KERNEL_INSTALL_PATH) ..."; \
		mkdir -p $(KERNEL_INSTALL_PATH); \
		$(MAKE) $(AMBA_MAKE_PARA) $(LINUX_MAKE_FLAG) $(LINUX_INSTALL_FLAG) modules_install; \
		find $(KERNEL_INSTALL_PATH)/lib/modules/ -name build | xargs -l1 rm -rf; \
		find $(KERNEL_INSTALL_PATH)/lib/modules/ -name source | xargs -l1 rm -rf; \
	fi
	@echo "Build $@ Done."

$(call add-target-into-build, linux)

###

.PHONY: menuconfig_public_linux

menuconfig_public_linux:
	@mkdir -p $(LINUX_OUT_DIR)
	$(AMBA_MAKEFILE_V)$(MAKE) $(AMBA_MAKE_PARA) $(LINUX_MAKE_FLAG) menuconfig

###

.PHONY: defconfig_public_linux

defconfig_public_linux:
	@echo "Build Linux with ambarella_$(KERNEL_DEFCONFIG)_defconfig ..."
	@mkdir -p $(LINUX_OUT_DIR)
	$(AMBA_MAKEFILE_V)$(MAKE) $(AMBA_MAKE_PARA) $(LINUX_MAKE_FLAG) ambarella_$(KERNEL_DEFCONFIG)_defconfig

