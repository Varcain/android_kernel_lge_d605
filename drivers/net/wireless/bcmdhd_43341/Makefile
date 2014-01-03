# bcmdhd
#####################
# SDIO Basic feature
#####################

DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER                     \
	-DDHD_DEBUG -DBDC -DTOE                                               \
	-DSHOW_EVENTS -DPROP_TXSTATUS -DBCMDBG                                \
	-DCUSTOMER_HW2                                                        \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                                \
	-DKEEP_ALIVE -DGET_CUSTOM_MAC_ENABLE -DPKT_FILTER_SUPPORT             \
	-DPNO_SUPPORT -DBCMDONGLEHOST                                         \
	-DDHD_USE_IDLECOUNT -DSET_RANDOM_MAC_SOFTAP -DROAM_ENABLE -DVSDB      \
	-DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST                            \
	-DESCAN_RESULT_PATCH -DPASS_ARP_PACKET -DSDIO_CRC_ERROR_FIX           \
	-DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT                           \
	-DCUSTOM_SDIO_F2_BLKSIZE=128 -DSUPPORT_PM2_ONLY                       \
	-DMIRACAST_AMPDU_SIZE=8

DHDOFILES = src/shared/aiutils.o           src/bcmsdio/sys/bcmsdh_sdmmc_linux.o      \
            src/dhd/sys/dhd_linux.o        src/shared/siutils.o                      \
            src/shared/bcmutils.o          src/dhd/sys/dhd_linux_sched.o             \
            src/dhd/sys/dhd_sdio.o         src/shared/bcmwifi/src/bcmwifi_channels.o \
            src/shared/bcmevent.o          src/shared/hndpmu.o                       \
            src/bcmsdio/sys/bcmsdh.o       src/dhd/sys/dhd_cdc.o                     \
	    src/bcmsdio/sys/bcmsdh_linux.o src/dhd/sys/dhd_common.o                  \
	    src/dhd/sys/dhd_wlfc.o         src/dhd/sys/dhd_ip.o                      \
	    src/shared/linux_osl.o         src/bcmsdio/sys/bcmsdh_sdmmc.o            \
	    src/dhd/sys/dhd_custom_gpio.o  src/shared/sbutils.o                      \
	    src/wl/sys/wldev_common.o      src/wl/sys/wl_android.o
	    

obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)

ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT -DUSE_IW
endif
ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += src/wl/sys/wl_cfg80211.o  src/wl/sys/wl_cfgp2p.o      \
               src/wl/sys/wl_linux_mon.o src/dhd/sys/dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211 -DWL_CFG80211_STA_EVENT -DWL_ENABLE_P2P_IF
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-75
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=10
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=55000
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=2
endif
ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif

#################
# Common feature
#################

DHDCFLAGS += -DCUSTOMER_HW4
DHDCFLAGS += -DCUSTOMER_HW10

ifeq ($(CONFIG_BCMDHD_HOSTWAKEUP_USE_GPIO),y)
ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
DHDCFLAGS += -DHW_OOB
endif
DHDCFLAGS += -DOOB_INTR_ONLY
DHDCFLAGS += -DCUSTOM_OOB_GPIO_NUM=$(CONFIG_BCMDHD_GPIO_WL_HOSTWAKEUP)
else
DHDCFLAGS += -DSDIO_ISR_THREAD
endif

ifeq ($(CONFIG_WIFI_CONTROL_FUNC),y)
DHDCFLAGS += -DCONFIG_WIFI_CONTROL_FUNC
endif

# Debug
#DHDCFLAGS += -DSIMPLE_MAC_PRINT
DHDCFLAGS += -DDEBUGFS_CFG80211
#DHDCFLAGS += -DSDTEST
# Print out kernel panic point of file and line info when assertion happened
DHDCFLAGS += -DBCMASSERT_LOG

# Wi-Fi Direct
DHDCFLAGS += -DWL_SCB_TIMEOUT=10

# CCX
ifeq ($(CONFIG_BRCM_CCX),y)
  DHDCFLAGS += -DBCMCCX
endif

# Early suspend
#DHDCFLAGS += -DDHD_USE_EARLYSUSPEND

# WiFi turn off delay
DHDCFLAGS += -DWIFI_TURNOFF_DELAY=100

# For Static Buffer
ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
  DHDCFLAGS += -DENHANCED_STATIC_BUF
  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
endif

# DTIM listen interval in suspend mode(0 means follow AP's DTIM period)
DHDCFLAGS += -DCUSTOM_SUSPEND_BCN_LI_DTIM=3

# Ioctl timeout 5000ms
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# DPC priority
DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=98

# Priority mismatch fix with kernel stack
DHDCFLAGS += -DPKTPRIO_OVERRIDE

# Config PM Control
#DHDCFLAGS += -DCONFIG_CONTROL_PM

#################
# JellyBean Plus
#################
DHDCFLAGS += -DWL_SUPPORT_BACKPORTED_KPATCHES

#########################
# Chip dependent feature
#########################

ifneq ($(CONFIG_BCM43341),)
  DHDCFLAGS += -DBCM43341_CHIP
  DHDCFLAGS += -DCONFIG_PM_LOCK
  DHDCFLAGS += -DENABLE_BCN_LI_BCN_WAKEUP
  # tput enhancement
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=8 -DCUSTOM_RXCHAIN=1
  DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=128
  DHDCFLAGS += -DBCMSDIOH_TXGLOM -DCUSTOM_TXGLOM=1 -DBCMSDIOH_TXGLOM_HIGHSPEED
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DREPEAT_READFRAME
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
  DHDCFLAGS += -DPROP_TXSTATUS_VSDB
ifeq ($(CONFIG_ARCH_MSM8974),y)
  DHDCFLAGS += -DCUSTOM_DEF_TXGLOM_SIZE=32 -DDHD_TXBOUND=32
endif
  DHDCFLAGS += -DCUSTOM_MAX_TXGLOM_SIZE=32
  DHDCFLAGS += -DDUAL_ESCAN_RESULT_BUFFER
  DHDCFLAGS += -DROAM_AP_ENV_DETECTION
  DHDCFLAGS += -DWL_CFG80211_GON_COLLISION
  DHDCFLAGS += -DDHCP_SCAN_SUPPRESS
endif

#############################
# Platform dependent feature
#############################
#For INITIAL 2G scan features
#DHDCFLAGS += -DUSE_INITIAL_2G_SCAN

#DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD

EXTRA_CFLAGS = $(DHDCFLAGS)

#########
# Others
#########
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif
EXTRA_CFLAGS += -DSRCBASE=\"$(src)/src\"
EXTRA_CFLAGS += -I$(src)/src/include/
EXTRA_CFLAGS += -I$(src)/src/dhd/sys/
EXTRA_CFLAGS += -I$(src)/src/dongle/
EXTRA_CFLAGS += -I$(src)/src/bcmsdio/sys/
EXTRA_CFLAGS += -I$(src)/src/wl/sys/
EXTRA_CFLAGS += -I$(src)/src/shared/
EXTRA_CFLAGS += -I$(src)/src/shared/bcmwifi/include/
EXTRA_CFLAGS += -I$(src)/src/shared/bcmwifi/src/

KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(shell pwd)


all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules

clean:
	rm -rf *.o *.ko *.mod.c *~ .*.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin \
	src/bcmsdio/sys/*.o \
	src/bcmsdio/sys/*.o.cmd \
	src/bcmsdio/sys/.*.o.cmd \
	src/dhd/sys/*.o \
	src/dhd/sys/*.o.cmd \
	src/dhd/sys/.*.o.cmd \
	src/shared/*.o \
	src/shared/*.o.cmd \
	src/shared/.*.o.cmd \
	src/wl/sys/*.o \
	src/wl/sys/*.o.cmd \
	src/wl/sys/.*.o.cmd \
	src/shared/bcmwifi/src/bcmwifi_channels.o \
	src/shared/bcmwifi/src/.*.o.cmd

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
