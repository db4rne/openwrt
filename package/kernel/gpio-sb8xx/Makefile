#
# Copyright (C) 2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gpio-sb8xx
PKG_RELEASE:=1

PKG_MAINTAINER:=Arne Zachlod <az@zachlod.eu>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gpio-sb8xx
  SUBMENU:=Other modules
  TITLE:= GPIO support for AMD FCH
  DEPENDS:= @GPIO_SUPPORT @TARGET_x86
  FILES:=$(PKG_BUILD_DIR)/gpio-sb8xx.ko
  AUTOLOAD:=$(call AutoLoad,30,gpio-sb8xx,1)
  KCONFIG:=
endef

define KernelPackage/gpio-sb8xx/description
  Support for GPIO functionality of AMD FCH.
endef

EXTRA_KCONFIG:= \
	CONFIG_GPIO_SB8XX=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

define Build/Compile
	 $(KERNEL_MAKE) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		$(EXTRA_KCONFIG) \
		modules
endef

$(eval $(call KernelPackage,gpio-sb8xx))
