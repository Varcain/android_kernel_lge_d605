
//                                                                
#define MAX_BRIGHTNESS_LM3639 			0xFF
//                                                                
#define DEFAULT_BRIGHTNESS 				0x73
#define DEFAULT_FTM_BRIGHTNESS			0x2B

struct lm3639_reg_data {
	char addr;
	char value;
};

void lm3639_reg_write_(struct lm3639_reg_data *);
void lm3639_reg_read_(struct lm3639_reg_data *);

struct lm3639_level2reg {
	char level;
	char reg_value;
};

const static struct lm3639_level2reg lm3639_bright_per_2reg_map[] =
{
	{0, 0xa},
	{20, 0xc},
	{40, 0x15},
	{48, 0x1d},
	{60, 0x24},
	{80, 0x3e},
	{100, 0x61},
};

