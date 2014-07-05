/*
** =========================================================================
** File:
**     ImmVibeSPI.c
**
** Description: 
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved. 
**
** This file contains Original Code and/or Modifications of Original Code 
** as defined in and that are subject to the GNU Public License v2 - 
** (the 'License'). You may not use this file except in compliance with the 
** License. You should have received a copy of the GNU General Public License 
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact 
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are 
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS 
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see 
** the License for the specific language governing rights and limitations 
** under the License.
** =========================================================================
*/
//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
#include <linux/io.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/regulator/msm-gpio-regulator.h>

#include <mach/irqs.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>
#include "../../../arch/arm/mach-msm/lge/fx3/board-fx3.h"
#include <linux/delay.h>
#else
#include <tspdrv_util.h>
extern struct pm8xxx_vib *vib_dev;
#endif
//                                                                             

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

#define PWM_DUTY_MAX    579 /* 13MHz / (579 + 1) = 22.4kHz */

static bool g_bAmpEnabled = false;

//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
/* gpio and clock control for vibrator */
#define REG_WRITEL(value, reg)				writel(value, (MSM_CLK_CTL_BASE + reg))
#define REG_READL(reg)						readl((MSM_CLK_CTL_BASE + reg))

#define GPn_MD_REG(n)						(0x2D00+32*(n))
#define GPn_NS_REG(n)						(0x2D24+32*(n))

#define GPIO_HAPTIC_EN			6
#define GPIO_HAPTIC_PWR_EN		54

/*  setting infomation GPIO with gp_clk.
** gp_clk_0a (GPIO 3): GPIOMUX_FUNC_2
** gp_clk_1a (GPIO 4): GPIOMUX_FUNC_3
** gp_clk_2a (GPIO 52): GPIOMUX_FUNC_2
** gp_clk_0b (GPIO 54): GPIOMUX_FUNC_2
** gp_clk_1b (GPIO 70): GPIOMUX_FUNC_2
** gp_clk_2b (GPIO 37): GPIOMUX_FUNC_5
*/
#define GPIO_LIN_MOTOR_PWM		37
#define GP_CLK_ID				2 /* gp clk 0 */

#define GP_CLK_M_DEFAULT		1
#define GP_CLK_N_DEFAULT		166  // 163 for test   kdw
#define GP_CLK_D_MAX			GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF			(GP_CLK_N_DEFAULT >> 1)

static struct msm_xo_voter *vib_clock;
static int vibrator_clock_init(void)
{
    int rc;
    /*Vote for XO clock*/
    vib_clock = msm_xo_get(MSM_XO_TCXO_D0, "vib_clock");

    if (IS_ERR(vib_clock)) {
        rc = PTR_ERR(vib_clock);
        printk(KERN_ERR "%s: Couldn't get TCXO_D0 vote for Vib(%d)\n",
                __func__, rc);
    }
    return rc;
}

static int vibrator_clock_on(void)
{
    int rc;
    rc = msm_xo_mode_vote(vib_clock, MSM_XO_MODE_ON);
    if (rc < 0) {
        printk(KERN_ERR "%s: Failed to vote for TCX0_D0 ON (%d)\n",
                __func__, rc);
    }
    return rc;
}

static int vibrator_clock_off(void)
{
    int rc;
    rc = msm_xo_mode_vote(vib_clock, MSM_XO_MODE_OFF);
    if (rc < 0) {
        printk(KERN_ERR "%s: Failed to vote for TCX0_D0 OFF (%d)\n",
                __func__, rc);
    }
    return rc;
}

static int vibratror_pwm_gpio_OnOFF(int OnOFF)
{
    int rc = 0;

    if (OnOFF) {
		rc = gpio_request(GPIO_LIN_MOTOR_PWM, "lin_motor_pwm");
		if (unlikely(rc < 0)) {
			printk("%s:GPIO_LIN_MOTOR_PWM(%d) request failed(%d)\n", __func__, GPIO_LIN_MOTOR_PWM, rc);
			return rc;
		}
    } else {
        gpio_free(GPIO_LIN_MOTOR_PWM);
    }
    return 0;
}

static void vibrator_power(int on)
{
	gpio_set_value(GPIO_HAPTIC_PWR_EN, on);
    mdelay(5);
}

static void vibrator_pwm_set(int enable, int amp, int n_value)
{
    uint M_VAL	= GP_CLK_M_DEFAULT;
	uint D_VAL	= GP_CLK_D_MAX;
	uint D_INV	= 0; /* QCT support invert bit for msm8960 */

	amp = (amp + 127)/2; // for test   kdw

	if (enable)
	{
        vibrator_clock_on();

		// for test   kdw
		//D_VAL = (((GP_CLK_D_MAX -1) * amp) >> 8) + GP_CLK_D_HALF;
		D_VAL = ((GP_CLK_D_MAX * amp) >> 7) ;

		if (D_VAL > GP_CLK_D_HALF)
		{
			if (D_VAL == GP_CLK_D_MAX)
			{      /* Max duty is 99% */
				D_VAL = 2;
			}
			else
			{
				D_VAL = GP_CLK_D_MAX - D_VAL;
			}

			D_INV = 1;
		}

		REG_WRITEL(
			(((M_VAL & 0xffU) << 16U) + 	/* M_VAL[23:16] */
			((~(D_VAL << 1)) & 0xffU)),		/* D_VAL[7:0] */
			GPn_MD_REG(GP_CLK_ID));

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(1U << 11U) +  				/* CLK_ROOT_ENA[11]		: Enable(1) */
			((D_INV & 0x01U) << 10U) +	/* CLK_INV[10]			: Disable(0) */
			(1U << 9U) +				/* CLK_BRANCH_ENA[9]	: Enable(1) */
			(1U << 8U) +				/* NMCNTR_EN[8]			: Enable(1) */
			(0U << 7U) +				/* MNCNTR_RST[7]		: Not Active(0) */
			(2U << 5U) +				/* MNCNTR_MODE[6:5]		: Dual-edge mode(2) */
			(3U << 3U) +				/* PRE_DIV_SEL[4:3]		: Div-4 (3) */
			(5U << 0U)), 				/* SRC_SEL[2:0]			: CXO (5) */
			GPn_NS_REG(GP_CLK_ID));
	}
	else
	{
        vibrator_clock_off();
		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(0U << 11U) +	/* CLK_ROOT_ENA[11]		: Disable(0) */
			(0U << 10U) +	/* CLK_INV[10]			: Disable(0) */
			(0U << 9U) +	/* CLK_BRANCH_ENA[9]	: Disable(0) */
			(0U << 8U) +	/* NMCNTR_EN[8]			: Disable(0) */
			(0U << 7U) +	/* MNCNTR_RST[7]		: Not Active(0) */
			(2U << 5U) +	/* MNCNTR_MODE[6:5]		: Dual-edge mode(2) */
			(3U << 3U) +	/* PRE_DIV_SEL[4:3]		: Div-4 (3) */
			(5U << 0U)),	/* SRC_SEL[2:0]			: CXO (5) */
			GPn_NS_REG(GP_CLK_ID));

    }
}

static void vibrator_ic_enable_set(int enable)
{
	gpio_set_value(GPIO_HAPTIC_EN, enable);
}
#endif
//                                                                             

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
    if (g_bAmpEnabled)
    {
#if !defined (CONFIG_MACH_LGE_L9II_COMMON)
        DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpDisable.\n"));
#else
	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpDisable.\n"));
#endif

//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
		vibrator_ic_enable_set(0);
		vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
		vibratror_pwm_gpio_OnOFF(0);
		vibrator_power(0);
#else
		pm8xxx_vib_set(vib_dev, 0, 0);
#endif
//                                                                             
		g_bAmpEnabled = false;
    }

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
    if (!g_bAmpEnabled)
    {
#if !defined (CONFIG_MACH_LGE_L9II_COMMON)
        DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpEnable.\n"));
#else
	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpEnable.\n"));
#endif

//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
		vibrator_power(1);
        vibratror_pwm_gpio_OnOFF(1);
		vibrator_pwm_set(1, 0, GP_CLK_N_DEFAULT);
		vibrator_ic_enable_set(1);
#else
		pm8xxx_vib_set(vib_dev, 1, 0);
#endif
//                                                                             
        g_bAmpEnabled = true;
    }

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
    int rc;
#endif

    printk("[VIBRATOR] %s\n", __func__);

    g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

#if defined (CONFIG_MACH_LGE_L9II_COMMON)
	rc = gpio_request(GPIO_HAPTIC_PWR_EN, "haptic_pwr_en");
	if (unlikely(rc < 0)) {
		printk("%s:GPIO_HAPTIC_PWR_EN(%d) request failed(%d)\n", __func__, GPIO_HAPTIC_PWR_EN, rc);
		return rc;
	}
	gpio_direction_output(GPIO_HAPTIC_PWR_EN, 0);

	rc = gpio_request(GPIO_HAPTIC_EN, "haptic_en");
	if (unlikely(rc < 0)) {
		printk("%s:GPIO_HAPTIC_EN(%d) request failed(%d)\n", __func__, GPIO_HAPTIC_EN, rc);
		return rc;
	}
	gpio_direction_output(GPIO_HAPTIC_EN, 0);

	rc = gpio_request(GPIO_LIN_MOTOR_PWM, "lin_motor_pwm");
	if (unlikely(rc < 0)) {
		printk("%s:GPIO_LIN_MOTOR_PWM(%d) request failed(%d)\n", __func__, GPIO_LIN_MOTOR_PWM, rc);
		return rc;
	}

    vibrator_clock_init();
#endif	

    /* 
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);

    return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
#if !defined (CONFIG_MACH_LGE_L9II_COMMON)
    DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Terminate.\n"));
#else
    DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_Terminate.\n"));
#endif

    /* 
    ** Disable amp.
    ** If multiple actuators are supported, please make sure to call
    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
    ** input argument).
    */
    ImmVibeSPI_ForceOut_AmpDisable(0);

//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
#if 0
    /* Set PWM frequency */
    PWM_CTRL  = 0;                  /* 13Mhz / (0 + 1) = 13MHz */
    PWM_PERIOD = PWM_DUTY_MAX;      /* 13Mhz / (PWM_DUTY_MAX + 1) = 22.4kHz */

    /* Set duty cycle to 50% */
    PWM_DUTY = (PWM_DUTY_MAX+1)>>1; /* Duty cycle range = [0, PWM_DUTY_MAX] */
#endif
#endif
//                                                                             

    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
    VibeInt8 nForce;

//    g_bStarted = true;

    switch (nOutputSignalBitDepth)
    {
        case 8:
            /* pForceOutputBuffer is expected to contain 1 byte */
            if (nBufferSizeInBytes != 1) return VIBE_E_FAIL;

            nForce = pForceOutputBuffer[0];
            break;
        case 16:
            /* pForceOutputBuffer is expected to contain 2 byte */
            if (nBufferSizeInBytes != 2) return VIBE_E_FAIL;

            /* Map 16-bit value to 8-bit */
            nForce = ((VibeInt16*)pForceOutputBuffer)[0] >> 8;
            break;
        default:
            /* Unexpected bit depth */
            return VIBE_E_FAIL;
    }
//                                                                             
#if defined (CONFIG_MACH_LGE_L9II_COMMON)
	/* Check the Force value with Max and Min force value */
	if (nForce > 127)
		nForce = 127;
	if (nForce < -127)
		nForce = -127;

	vibrator_pwm_set(1, nForce, GP_CLK_N_DEFAULT);
#else
//                                                                             
    if (nForce <= 0)
    {                
        pm8xxx_vib_set(vib_dev, 0, 0);        
    }
    else
    {
        /* 0 ~ 31 */
        pm8xxx_vib_set(vib_dev, 1, (nForce * 31) / 128 + 1);
    }
//                                                                             
#endif
//                                                                             

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
#if 0
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    /* This function is not called for ERM device */

    return VIBE_S_SUCCESS;
}
#endif

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

#if !defined (CONFIG_MACH_LGE_L9II_COMMON)
    DbgOut((KERN_DEBUG "ImmVibeSPI_Device_GetName.\n"));
#else
    DbgOut((DBL_INFO, "ImmVibeSPI_Device_GetName.\n"));
#endif

    strncpy(szDevName, "FX3", nSize-1);
    szDevName[nSize - 1] = '\0';    /* make sure the string is NULL terminated */

    return VIBE_S_SUCCESS;
}
