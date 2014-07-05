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

#ifndef __TSPDRV_UTIL_H
#define __TSPDRV_UTIL_H

#include <linux/mfd/pm8xxx/core.h>

#define VIB_DRV         0x4A

#define VIB_DRV_SEL_MASK    0xf8
#define VIB_DRV_SEL_SHIFT   0x03
#define VIB_DRV_EN_MANUAL_MASK  0xfc
#define VIB_DRV_LOGIC_SHIFT 0x2

struct pm8xxx_vib {
    spinlock_t lock;
    struct device *dev;
    u8 reg_vib_drv;
};

static int pm8xxx_vib_read_u8(struct pm8xxx_vib *vib, u8 *data, u16 reg)
{
    int rc;

    rc = pm8xxx_readb(vib->dev->parent, reg, data);
    if (rc < 0)
        dev_warn(vib->dev, "Error reading pm8xxx: %X - ret %X\n", reg, rc);

    return rc;
}
static int pm8xxx_vib_write_u8(struct pm8xxx_vib *vib, u8 data, u16 reg)
{
    int rc;

    rc = pm8xxx_writeb(vib->dev->parent, reg, data);
    if (rc < 0)
        dev_warn(vib->dev, "Error writing pm8xxx: %X - ret %X\n", reg, rc);

    return rc;
}

static int pm8xxx_vib_set(struct pm8xxx_vib *vib, int on, int level)
{
    int rc;
    u8 val;
    unsigned long flags;

    spin_lock_irqsave(&vib->lock, flags);

    if (on) {
        val = vib->reg_vib_drv;
        val &= ~VIB_DRV_SEL_MASK;
        val |= ((level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
        rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
        if (rc < 0)
            return rc;
        vib->reg_vib_drv = val;
    } else {
        val = vib->reg_vib_drv;
        val &= ~VIB_DRV_SEL_MASK;
        rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);

        if (rc < 0)
            return rc;
        vib->reg_vib_drv = val;
    }

    spin_unlock_irqrestore(&vib->lock, flags);

    return rc;
}
#endif
