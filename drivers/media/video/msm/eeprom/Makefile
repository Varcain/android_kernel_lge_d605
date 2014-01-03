GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm/io
obj-$(CONFIG_MSM_EEPROM) += msm_camera_eeprom.o
# LGE_CHANGE
#obj-$(CONFIG_IMX074_EEPROM) += imx074_eeprom.o
#obj-$(CONFIG_IMX091_EEPROM) += imx091_eeprom.o
#obj-$(CONFIG_S5K4E1_EEPROM) += s5k4e1_eeprom.o
obj-$(CONFIG_S5K4E5YA_EEPROM) += s5k4e5ya_eeprom.o
obj-$(CONFIG_HI543_EEPROM) += hi543_eeprom.o
