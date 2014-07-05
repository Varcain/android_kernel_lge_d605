/*
  * Copyright (C) 2012 LGE, Inc.
  *
  * This software is licensed under the terms of the GNU General Public
  * License version 2, as published by the Free Software Foundation, and
  * may be copied, distributed, and modified under those terms.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  */

#ifndef __LINUX_DIRECT_QCOIN_H
#define __LINUX_DIRECT_QCOIN_H

#define PM8XXX_VIBRATOR_DEV_NAME "pm8xxx-vib"

#define GAIN_MAX 128
#define GAIN_MIN 0

/* direct qcoin platform data */
struct direct_qcoin_platform_data {
	int enable_status;
	int amp;
        int (*power_set)(struct device *dev, int enable);
	int high_max;
	int low_max;
	int low_min;
};

#endif
