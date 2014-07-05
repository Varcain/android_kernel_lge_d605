#
# Makefile for nfc devices
#

obj-$(CONFIG_LGE_NFC)	:= pn544_lge.o
obj-$(CONFIG_LGE_NFC)	+= pn544_lge_hwadapter.o
obj-$(CONFIG_NFC_BCM43341)	+= bcm2079x-i2c.o
obj-$(CONFIG_PN544_NFC)		+= pn544.o
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_BCM2079X)		+= bcm2079x-i2c.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
