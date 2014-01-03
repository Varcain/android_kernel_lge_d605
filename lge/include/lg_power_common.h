/* 
                       
  
                                                           
*/

#ifdef CONFIG_LGE_PM_CURRENT_CABLE_TYPE
typedef enum {
	NO_INIT_CABLE,
	MHL_CABLE_500MA,
	TA_CABLE_600MA,
	TA_CABLE_800MA,
	TA_CABLE_DTC_800MA,	/* desk-top cradle */
	TA_CABLE_FORGED_500MA,
	LT_CABLE_56K,
	LT_CABLE_130K,
	USB_CABLE_400MA,	
	USB_CABLE_DTC_500MA,/* desk-top cradle */
	ABNORMAL_USB_CABLE_400MA,	
//                                                                
	LT_CABLE_910K,
//                                                 
#ifdef CONFIG_MACH_LGE_I_BOARD_SKT
	TA_CABLE_NOT_AUTH_700MA,
    MAX_CABLE,
#else
    MAX_CABLE,
#endif
}acc_cable_type;
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#ifdef CONFIG_LGE_PM_CAYMAN_VZW
enum{
	THERM_M20,
	THERM_M10,
	THERM_M5,
	THERM_5,
	THERM_15,
	THERM_25,
	THERM_34,
	THERM_42,
	THERM_45,
	THERM_50,
	THERM_55,
	THERM_57,
	THERM_60,
	THERM_65,
	THERM_LAST,
};
#else
enum{
	THERM_M10,
	THERM_M5,
	THERM_42,
	THERM_45,
	THERM_55,
	THERM_57,
	THERM_60,
	THERM_65,
	THERM_LAST,
};
#endif
 
enum{
	DISCHG_BATT_TEMP_OVER_60,
	DISCHG_BATT_TEMP_57_60,
	DISCHG_BATT_TEMP_UNDER_57,
	CHG_BATT_TEMP_OVER_55,
	CHG_BATT_TEMP_46_55,
	CHG_BATT_TEMP_42_45,
	CHG_BATT_TEMP_M4_41,
	CHG_BATT_TEMP_M10_M5,
	CHG_BATT_TEMP_UNDER_M10,
};

enum{
	DISCHG_BATT_NORMAL_STATE,
	DISCHG_BATT_WARNING_STATE,
	DISCHG_BATT_POWEROFF_STATE,
	CHG_BATT_NORMAL_STATE,
	CHG_BATT_DC_CURRENT_STATE,
	CHG_BATT_WARNING_STATE,
	CHG_BATT_STOP_CHARGING_STATE,
};
#endif

#ifdef CONFIG_LGE_PM
struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};
#endif



