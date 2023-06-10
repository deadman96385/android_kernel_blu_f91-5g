#ifndef SC_CHIP_COMMON_H
#define SC_CHIP_COMMON_H
#include "SC_chip_custom.h"

#define	SC8XXX_60	0x01
#define	SC8XXX_61	0x02
#define	SC1699	         0x03
#define	SC6XX0		0x04
#define	SC6XX1		0x05
#define	SC7XXX		0x06
#define     SC6XX3             0x07

//#define SC_FLASH_I2C_ADDR		    0x2c//7bit addr
#define SC_FLASH_I2C_ADDR		    0x2c//8bit addr
#define I2C_WRITE		0x00
#define	I2C_READ		0x01

#ifdef	  CTP_USE_SW_I2C
#define   FLASH_WSIZE			    256//0x128
#define   FLASH_RSIZE			    64//0x128
#define   PROJECT_INFO_LEN          20// ProjectInfo字符个数
#endif

#ifdef	  CTP_USE_HW_I2C
#define   FLASH_WSIZE			    128//写操作最多8字节
#define   FLASH_RSIZE			    128//读操作最多8字节
#define   PROJECT_INFO_LEN          8// ProjectInfo字符个数
#endif

#define ARGU_MARK  "chp_cfg_mark"

enum ctp_type{
    SELF_CTP                = 0x00,
	COMPATIBLE_CTP          = 0x01,
	SELF_INTERACTIVE_CTP    = 0x02,
};

#if(TS_CHIP == SC8XXX_60)

#define	MAX_POINT_NUM	2
#define SC_FLASH_ID	0x20
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0x7b5b

#define I2C_TRANSFER_WSIZE	(128)
#define I2C_TRANSFER_RSIZE	(128)
#define I2C_VERFIY_SIZE		(128)

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	100

#define CTP_TYPE                COMPATIBLE_CTP

enum SC8XXX_60_flash_cmd {

	READ_MAIN_CMD		= 0x02,
	ERASE_ALL_MAIN_CMD	= 0x04,
	ERASE_PAGE_MAIN_CMD	= 0x06,
	WRITE_MAIN_CMD		= 0x08,
	RW_REGISTER_CMD		= 0x0a,
};

#elif(TS_CHIP == SC8XXX_61)

#define	MAX_POINT_NUM	5
#define SC_FLASH_ID	0x20
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define	I2C_TRANSFER_WSIZE      (64)
#define	I2C_TRANSFER_RSIZE      (64)
#define	I2C_VERFIY_SIZE         (64)

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	100

#define CTP_TYPE                COMPATIBLE_CTP

enum SC8XXX_61_flash_cmd {

	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == SC1699)

#define	MAX_POINT_NUM		10
#define	SC_FLASH_ID			0x05
#define	MAX_FLASH_SIZE		0xc000
#define	PJ_ID_OFFSET		0xcb

#define	I2C_TRANSFER_WSIZE		(256)
#define	I2C_TRANSFER_RSIZE		(256)
#define	I2C_VERFIY_SIZE			(256)

#define VERTIFY_START_OFFSET 	0x3fc
#define VERTIFY_END_OFFSET   	0x3fd

#define	FW_CHECKSUM_DELAY_TIME			50

#define CTP_TYPE                COMPATIBLE_CTP

enum SC1699_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == SC6XX0)

#define	MAX_POINT_NUM	2
#define SC_FLASH_ID	0x20
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define	I2C_TRANSFER_WSIZE	(256)
#define	I2C_TRANSFER_RSIZE	(256)
#define	I2C_VERFIY_SIZE		(256)

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP

enum SC6XX0_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == SC6XX1)

#define	MAX_POINT_NUM	2
//#define SC_FLASH_ID	0x21
#define SC_FLASH_ID	0x05
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define	I2C_TRANSFER_WSIZE	(256)
#define	I2C_TRANSFER_RSIZE	(256)
#define	I2C_VERFIY_SIZE		(256)

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP

enum SC6XX1_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};

#elif(TS_CHIP == SC7XXX)

#define	MAX_POINT_NUM	5
#define SC_FLASH_ID	0x40
#define	MAX_FLASH_SIZE	0xc000
#define	PJ_ID_OFFSET	0xcb

#define	I2C_TRANSFER_WSIZE	(256)
#define	I2C_TRANSFER_RSIZE	(256)
#define	I2C_VERFIY_SIZE		(256)

#define VERTIFY_START_OFFSET 0x23
#define VERTIFY_END_OFFSET   0x24

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE             SELF_INTERACTIVE_CTP

enum SC7XXX_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#elif(TS_CHIP == SC6XX3)

#define	MAX_POINT_NUM	1
#define SC_FLASH_ID	0x22
#define	MAX_FLASH_SIZE	0x8000
#define	PJ_ID_OFFSET	0xcb

#define	I2C_TRANSFER_WSIZE	(256)
#define	I2C_TRANSFER_RSIZE	(256)
#define	I2C_VERFIY_SIZE		(256)

#define VERTIFY_START_OFFSET 0x3fc
#define VERTIFY_END_OFFSET   0x3fd

#define	FW_CHECKSUM_DELAY_TIME	250

#define CTP_TYPE                SELF_CTP

enum SC6XX3_flash_cmd {
	
	ERASE_SECTOR_MAIN_CMD	= 0x06,
	ERASE_ALL_MAIN_CMD	= 0x09,	
	RW_REGISTER_CMD		= 0x0a,
	READ_MAIN_CMD		= 0x0D,
	WRITE_MAIN_CMD		= 0x0F,
	WRITE_RAM_CMD		= 0x11,
	READ_RAM_CMD		= 0x12,
};
#endif

enum fw_reg {

	WORK_MODE_REG		= 0x00,
    TS_DATA_REG         = 0x85,
	CHECKSUM_REG		= 0x3f,
	CHECKSUM_CAL_REG	= 0x8a,
	AC_REG				= 0x8b,
	RESOLUTION_REG		= 0x98,
	LPM_REG				= 0xa5,
	PROXIMITY_REG		= 0xb0,
	PROXIMITY_FLAG_REG	= 0xB1,
	CALL_REG			= 0xb2,
	SC_CHIP_ID_REG      = 0xb8,
	SC_FWVER_PJ_ID_REG  = 0xb6,
	SC_PRJ_INFO_REG     = 0xb4,
	CHIP_ID_REG         = 0xe7,
	SC_PRJ_ID_REG       = 0xb5,
	COB_ID_REG          = 0x33,
};


enum work_mode {

	NORMAL_MODE		= 0x00,
	FACTORY_MODE		= 0x40,
};

enum lpm {

	ACTIVE_MODE		= 0x00,
	MONITOR_MODE		= 0x01,
	STANDBY_MODE		= 0x02,
	SLEEP_MODE		= 0x03,
	GESTURE_MODE		= 0x04,

};

enum checksum {

	CHECKSUM_READY		= 0x01,
	CHECKSUM_CAL		= 0xaa,
	CHECKSUM_ARG		= 0xba,
};

enum update_type{
	
	NONE_UPDATE		= 0x00,
	ARG_UPDATE		= 0x01,
	FW_ARG_UPDATE   = 0x02,
};

enum firmware_file_type{
	
	HEADER_FILE_UPDATE		= 0x00,
	BIN_FILE_UPDATE		= 0x01,
};

enum update_mode{
    COF_UPDATE    = 0x00,
	COB_UPDATE    = 0x01,
};

typedef enum 
{
    CTP_DOWN = 3,
    CTP_UP   = 4,
    CTP_MOVE = 5,
    CTP_RESERVE = 0,
} ctp_pen_state_enum;

#define  TD_STAT_ADDR		    0x1
#define  TD_STAT_NUMBER_TOUCH	0x07
#define  CTP_PATTERN	            0xAA

#define OUTPUT	1
#define INPUT	0
/*************SunnyCore ic update***********/
#ifdef      SC_UPDATE_FIRMWARE_ENABLE
#if(TS_CHIP == SC7XXX)
#define 	SC_ARGUMENT_BASE_OFFSET	(0xbc00)
#else
#define 	SC_ARGUMENT_BASE_OFFSET	(0x200)
#endif
#define 	SC_FWVER_MAIN_OFFSET	(0x2a)
#define 	SC_FWVER_ARGU_OFFSET	(0x2b)
#define 	SC_PROJECT_ID_OFFSET	(0x2c)
#define     SC_PROJECT_INFO_OFFSET  (0x0f)
#define     SC_COB_ID_OFFSET        (0x34)
#define     SC_COB_ID_LEN           (12)
#define     SC_ARGUMENT_FLASH_SIZE  (1024)
#define     PRJ_INFO_LEN            (0x08)
#define     FLASH_PAGE_SIZE         (512)
//Update firmware through driver probe procedure with h and c file
#define 	SC_AUTO_UPDATE_FARMWARE
#ifdef		SC_AUTO_UPDATE_FARMWARE

#endif

//Update firmware through 
//#define     SC_UPDATE_FARMWARE_WITH_BIN
#ifdef      SC_UPDATE_FARMWARE_WITH_BIN
#define SC_FIRMWARE_BIN_PATH    "/sdcard/SunnyCore_firmware.bin"//"/system/etc/SunnyCore_firmware.bin"
#endif

#endif

/*************SunnyCore ic virtrual key***********/
#if defined(SC_VIRTRUAL_KEY_SUPPORT)
#define	SC_VIRTRUAL_KEY1_X	80
#define	SC_VIRTRUAL_KEY1_Y	900
#define	SC_VIRTRUAL_KEY1	KEY_MENU

#define	SC_VIRTRUAL_KEY2_X	240
#define	SC_VIRTRUAL_KEY2_Y	900
#define	SC_VIRTRUAL_KEY2	KEY_HOMEPAGE

#define	SC_VIRTRUAL_KEY3_X	400
#define	SC_VIRTRUAL_KEY3_Y	900
#define	SC_VIRTRUAL_KEY3	KEY_BACK
#endif
/*************SunnyCore ic debug***********/
#if defined(SC_DEBUG_SUPPORT)
#define SC_DEBUG_ON          1
#define SC_DEBUG_ARRAY_ON    0
#define SC_DEBUG_FUNC_ON     1
#define SC_INFO(fmt,arg...)           printk("<<-sunnycore-INFO->> "fmt"\n",##arg)
#define SC_ERROR(fmt,arg...)          printk("<<-sunnycore-ERROR->> "fmt"\n",##arg)
#define SC_DEBUG(fmt,arg...)          do{\
                                         if(SC_DEBUG_ON)\
                                         printk("<<-sunnycore-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
                                       }while(0)
#define SC_DEBUG_ARRAY(array, num)    do{\
                                         s32 i;\
                                         u8* a = array;\
                                         if(SC_DEBUG_ARRAY_ON)\
                                         {\
                                            printk("<<-sunnycore-DEBUG-ARRAY->>\n");\
                                            for (i = 0; i < (num); i++)\
                                            {\
                                                printk("%02x   ", (a)[i]);\
                                                if ((i + 1 ) %10 == 0)\
                                                {\
                                                    printk("\n");\
                                                }\
                                            }\
                                            printk("\n");\
                                        }\
                                       }while(0)
#define SC_DEBUG_FUNC()               do{\
                                         if(SC_DEBUG_FUNC_ON)\
                                         printk("<<-sunnycore-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__);\
                                       }while(0)
#else
#define SC_INFO(fmt,arg...)
#define SC_ERROR(fmt,arg...)
#define SC_DEBUG(fmt,arg...)
#define SC_DEBUG_ARRAY(array, num)

#define SC_DEBUG_FUNC() 
#endif

#endif
