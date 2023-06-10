/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 S5Kgw1mipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6735
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *  20150624: the first driver from 
 *  20150706: add pip 15fps setting
 *  20150716: 更新log的打印方法
 *  20150720: use non - continue mode
 *  15072011511229: add pdaf, the pdaf old has be delete by recovery
 *  15072011511229: add 旧的log兼容，新的log在这个版本不能打印log？？
 *  15072209190629: non - continue mode bandwith limited , has <tiaowen> , modify to continue mode
 *  15072209201129: modify not enter init_setting bug
 *  15072718000000: crc addd 0x49c09f86
 *  15072718000001: MODIFY LOG SWITCH
 *  15072811330000: ADD NON-CONTIUE MODE ,PREVIEW 29FPS,CAPTURE 29FPS
 					([TODO]REG0304 0786->0780  PREVEIW INCREASE TO 30FPS)
 *  15072813000000: modify a wrong setting at pip reg030e 0x119->0xc8
 *  15080409230000: pass!
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "s5kgw1spmipiraw_Sensor.h"
/*===FEATURE SWITH===*/
//#define FPTPDAFSUPPORT   //for pdaf switch
//#define LOG_INF LOG_INF_LOD
//#define NONCONTINUEMODE
/*===FEATURE SWITH===*/
#define PFX "S5KGW1SP_camera_sensor"
//#define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
//#defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)	pr_err(PFX "[%s] " format, __func__, ##args)
//static kal_uint32 streaming_control(kal_bool enable);
//#define SENSORDB LOG_INF
/****************************   Modify end    *******************************************/
//static bool bIsLongExposure = KAL_FALSE;

//extern bool s5kgw1spCheckLensVersion(int id);
// extern void s5kgw1sp_MIPI_update_wb_register_from_otp(void);

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGW1_SENSOR_ID,		//Sensor ID Value: 0x30C8//record sensor id defined in Kd_imgsensor.h
	.checksum_value = 0x362cca75,		//checksum value for Camera Auto Test
	.pre = {
		.pclk = 1920000000,				//record different mode's pclk
		.linelength  = 33712,				//record different mode's linelength
		.framelength = 3582,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4640,		//record different mode's width of grabwindow
		.grabwindow_height = 3472,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 921600000,
		.max_framerate = 150,    			
	},
	.cap = {
		.pclk = 1920000000,				//record different mode's pclk
		.linelength  = 33712,				//record different mode's linelength
		.framelength = 2582,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4640,		//record different mode's width of grabwindow
		.grabwindow_height = 3472,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 921600000,
		.max_framerate = 150,    			
	},
	.normal_video = {
		.pclk = 1920000000,				//record different mode's pclk
		.linelength  = 33712,				//record different mode's linelength
		.framelength = 3582,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4640,		//record different mode's width of grabwindow
		.grabwindow_height = 3472,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 921600000,
		.max_framerate = 150,    			
	},
	.hs_video = {
		.pclk = 1920000000,				//record different mode's pclk //linelength * framelength * max_framerate
		.linelength  = 13888,				//record different mode's linelength
		.framelength = 1152,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 1920	,		//record different mode's width of grabwindow
		.grabwindow_height = 1080,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 366000000,
		.max_framerate = 1200,	
	},
	.slim_video = {
		.pclk = 1920000000,				//record different mode's pclk
		.linelength  = 16448,				//record different mode's linelength
		.framelength = 3890,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4640,		//record different mode's width of grabwindow
		.grabwindow_height = 3472,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 921600000,
		.max_framerate = 300,   
	},
	.custom1 = {
		.pclk = 1920000000,				//record different mode's pclk
		.linelength  = 16448,				//record different mode's linelength
		.framelength = 3890,			//record different mode's framelength
		.startx= 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4640,		//record different mode's width of grabwindow
		.grabwindow_height = 3472,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 921600000,
		.max_framerate = 300,    			
	},
//czy add
 	.min_gain = 64, /*1x gain*/
	.max_gain = 1024, /*16x gain*/
	.min_gain_iso = 100,
	.margin = 32, /* sensor framelength & shutter margin */
	.min_shutter = 2, /* min shutter */
 	.gain_step = 1,
	.gain_type = 0, 
//czy add
//	.margin = 8,			                //sensor framelength & shutter margin
//	.min_shutter = 4,		              //min shutter
	.max_frame_length = 0xFFFF,       //REG0x0202 <=REG0x0340-5//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	        //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,  //sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,      //isp gain delay frame for AE cycle
	.ihdr_support = 0,	              //1, support; 0,not support
	.ihdr_le_firstline = 0,           //1,le first ; 0, se first
	.sensor_mode_num = 5,	            //support sensor mode num ,don't support Slow motion
	.cap_delay_frame = 3,		//enter capture delay frame num
	.pre_delay_frame = 3, 		//enter preview delay frame num
	.video_delay_frame = 3,		//enter video delay frame num
	.hs_video_delay_frame = 3,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 3,//enter slim video delay frame num
	.custom1_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
  //  .mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
     .mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,//SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	//.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
	.i2c_addr_table = {0x5A,0x20,0x7a,0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
    .i2c_speed = 400, // i2c read/write speed
};
static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0, /* current shutter */
	.gain = 0x0100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
    /* Preview mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0D90, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x30, 0x02DA, 0x0d8C, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0D90, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x30, 0x02DA, 0x0d8C, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0D90, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x30, 0x02DA, 0x0d8C, 0x03, 0x00, 0x0000, 0x0000},
	};
/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =	 
{
 { 9280, 6944,	 0,    0,  9280, 6944, 4640, 3472,   0,	0, 4640, 3472, 	 0, 0, 4640, 3472}, // Preview 
 { 9280, 6944,	 0,    0,  9280, 6944, 4640, 3472,   0,	0, 4640, 3472, 	 0, 0, 4640, 3472}, // capture 
 { 9280, 6944,	 0,    0,  9280, 6944, 4640, 3472,   0,	0, 4640, 3472, 	 0, 0, 4640, 3472}, // Preview 
 { 9280, 6944, 800, 1312,  7680, 4320, 1920, 1080,   0, 0, 1920, 1080,   0, 0, 1920, 1080},   //high speed video
 { 9280, 6944,	 0,    0,  9280, 6944, 4640, 3472,   0,	0, 4640, 3472, 	 0, 0, 4640, 3472}, // Preview 
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
   .i4OffsetX = 0,
   .i4OffsetY = 0,
   .i4PitchX = 16,
   .i4PitchY = 16,
   .i4PairNum = 16,
   .i4SubBlkW = 8,
   .i4SubBlkH = 2, 
   .i4PosL = {{1, 0}, {9, 0}, {3, 3}, {11, 3}, {7, 4}, {15, 4}, {5, 7}, {13, 7},{1, 8}, {9, 8}, {3, 11}, {11, 11}, {7, 12}, {15, 12}, {5, 15}, {13, 15} },
   .i4PosR = {{0, 0}, {8, 0}, {2, 3}, {10, 3}, {6, 4}, {14, 4}, {4, 7}, {12, 7},{0, 8}, {8, 8}, {2, 11}, {10, 11}, {6, 12}, {14, 12}, {4, 15}, {12, 15} },
   .iMirrorFlip = 0,
   .i4BlockNumX = 290,
   .i4BlockNumY = 216,//[agold][yz] change 217 -> 216 for pd dump debug(ini 217)
};
static kal_uint16 read_cmos_sensor_byte(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    iReadRegI2C(pu_send_cmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    return get_byte;
}
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}
static void write_cmos_sensor_byte(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    iWriteRegI2C(pusendcmd , 4, imgsensor.i2c_write_id);
}

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 4 bytes */
static kal_uint16 s5kgw1_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
		if ((I2C_BUFFER_LEN - tosend) < 4
			|| IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						4,
						imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);	  
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
}	/*	set_dummy  */
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	LOG_INF("framerate = %d, min framelength should enable(%d) \n", framerate,min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length; 
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
		//imgsensor.dummy_line = 0;
	//else
		//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */
#if 1
static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;
	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_byte(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	LOG_INF(" check_streamoff exit!\n");
}
static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		//write_cmos_sensor(0x6214, 0x7970);
		write_cmos_sensor_byte(0x0100, 0X01);
	} else {
		//write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor_byte(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}
#endif
#if 0
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);	
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}
	// Update Shutter
	write_cmos_sensor(0x0202, (shutter) & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
	//LOG_INF("frame_length = %d ", frame_length);
}	/*	write_shutter  */
#endif
/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	//kal_uint32 frame_length = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	//write_shutter(shutter);
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);	
		else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}
	// Update Shutter
	write_cmos_sensor(0X0202, shutter & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	//
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			//write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length);
			//write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		// Extend frame length
		//write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340,  imgsensor.frame_length);
		//write_cmos_sensor(0x0104, 0x00);
	}

	// Update Shutter
	//write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor(0x0202, shutter);
	//write_cmos_sensor(0x0104, 0x00);
	printk("Add for N3D! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	//gain = 64 = 1x real gain.
    reg_gain = gain/2;
	//reg_gain = reg_gain & 0xFFFF;
	return (kal_uint16)reg_gain;
}
/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{	
  	//gain = 64 = 1x real gain.
	kal_uint16 reg_gain;
	LOG_INF("set_gain %d \n", gain);
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;		 
	}
    reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain; 
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));    
	return gain;
}	/*	set_gain  */
//ihdr_write_shutter_gain not support for S5K3m5
// static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
// {
// 	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
// 	if (imgsensor.ihdr_en) {
// 		spin_lock(&imgsensor_drv_lock);
// 			if (le > imgsensor.min_frame_length - imgsensor_info.margin)		
// 				imgsensor.frame_length = le + imgsensor_info.margin;
// 			else
// 				imgsensor.frame_length = imgsensor.min_frame_length;
// 			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
// 				imgsensor.frame_length = imgsensor_info.max_frame_length;
// 			spin_unlock(&imgsensor_drv_lock);
// 			if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
// 			if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;
// 		// Extend frame length first
// 		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
// 		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
// 		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
// 		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);	 
// 		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
// 		write_cmos_sensor(0x3512, (se << 4) & 0xFF); 
// 		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
// 		write_cmos_sensor(0x3510, (se >> 12) & 0x0F); 
// 		set_gain(gain);
// 	}
// }
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
	/********************************************************
	   *
	   *   0x3820[2] ISP Vertical flip
	   *   0x3820[1] Sensor Vertical flip
	   *
	   *   0x3821[2] ISP Horizontal mirror
	   *   0x3821[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/
	spin_lock(&imgsensor_drv_lock);
    imgsensor.mirror= image_mirror; 
    spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
		case IMAGE_NORMAL:
			write_cmos_sensor_byte(0x0101,0x00); //GR
			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor_byte(0x0101,0x01); //R
			break;
		case IMAGE_V_MIRROR:
			write_cmos_sensor_byte(0x0101,0x02); //B	
			break;
		case IMAGE_HV_MIRROR:
			write_cmos_sensor_byte(0x0101,0x03); //GB
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
			break;
	}
}
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
// static void night_mode(kal_bool enable)
// {
// /*No Need to implement this function*/ 
// }	/*	night_mode	*/
static void sensor_init(void)
{
  LOG_INF("E\n"); 
  #if 0
write_cmos_sensor(0xFCFC, 0x4000);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x0000, 0x0016);
write_cmos_sensor(0x0000, 0x0971);
write_cmos_sensor(0x6010, 0x0001);
mdelay(38);                       
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x0A02, 0x7E00);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xFB74);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0xFEB8);
write_cmos_sensor(0x602A, 0xFD78);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF041);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0xF8F9);
write_cmos_sensor(0x6F12, 0x20B9);
write_cmos_sensor(0x6F12, 0x0122);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x1648);
write_cmos_sensor(0x6F12, 0xFFF7);
write_cmos_sensor(0x6F12, 0xCAFB);
write_cmos_sensor(0x6F12, 0x164D);
write_cmos_sensor(0x6F12, 0x1548);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0xA863);
write_cmos_sensor(0x6F12, 0x6B6C);
write_cmos_sensor(0x6F12, 0x1549);
write_cmos_sensor(0x6F12, 0x1548);
write_cmos_sensor(0x6F12, 0x9847);
write_cmos_sensor(0x6F12, 0x154E);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0x1549);
write_cmos_sensor(0x6F12, 0xB063);
write_cmos_sensor(0x6F12, 0x6B6C);
write_cmos_sensor(0x6F12, 0x1548);
write_cmos_sensor(0x6F12, 0x9847);
write_cmos_sensor(0x6F12, 0xF064);
write_cmos_sensor(0x6F12, 0x1448);
write_cmos_sensor(0x6F12, 0x00F5);
write_cmos_sensor(0x6F12, 0xFC64);
write_cmos_sensor(0x6F12, 0xB0F8);
write_cmos_sensor(0x6F12, 0xE077);
write_cmos_sensor(0x6F12, 0xF069);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x8AF8);
write_cmos_sensor(0x6F12, 0x2080);
write_cmos_sensor(0x6F12, 0x6B6C);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0x1049);
write_cmos_sensor(0x6F12, 0x1048);
write_cmos_sensor(0x6F12, 0x9847);
write_cmos_sensor(0x6F12, 0xF061);
write_cmos_sensor(0x6F12, 0x0D49);
write_cmos_sensor(0x6F12, 0x2780);
write_cmos_sensor(0x6F12, 0x43F6);
write_cmos_sensor(0x6F12, 0x1000);
write_cmos_sensor(0x6F12, 0xC1F8);
write_cmos_sensor(0x6F12, 0xC407);
write_cmos_sensor(0x6F12, 0x0D49);
write_cmos_sensor(0x6F12, 0x0968);
write_cmos_sensor(0x6F12, 0x4883);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xF081);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFD74);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFE21);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x9B20);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFE61);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x63AD);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xF780);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFEF5);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x2773);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x4C00);
write_cmos_sensor(0x6F12, 0x2002);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0xCF27);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0C60);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF041);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0xADF9);
write_cmos_sensor(0x6F12, 0xC84D);
write_cmos_sensor(0x6F12, 0x4FF2);
write_cmos_sensor(0x6F12, 0x5E54);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x685C);
write_cmos_sensor(0x6F12, 0x78B1);
write_cmos_sensor(0x6F12, 0x01EB);
write_cmos_sensor(0x6F12, 0xC102);
write_cmos_sensor(0x6F12, 0x05EB);
write_cmos_sensor(0x6F12, 0x8202);
write_cmos_sensor(0x6F12, 0x0A32);
write_cmos_sensor(0x6F12, 0x2046);
write_cmos_sensor(0x6F12, 0x0023);
write_cmos_sensor(0x6F12, 0x4FF0);
write_cmos_sensor(0x6F12, 0x8046);
write_cmos_sensor(0x6F12, 0x32F8);
write_cmos_sensor(0x6F12, 0x027B);
write_cmos_sensor(0x6F12, 0x3752);
write_cmos_sensor(0x6F12, 0x5B1C);
write_cmos_sensor(0x6F12, 0x801C);
write_cmos_sensor(0x6F12, 0x122B);
write_cmos_sensor(0x6F12, 0xF6D3);
write_cmos_sensor(0x6F12, 0x2434);
write_cmos_sensor(0x6F12, 0x491C);
write_cmos_sensor(0x6F12, 0x0929);
write_cmos_sensor(0x6F12, 0xE9D3);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xF081);
write_cmos_sensor(0x6F12, 0x10B5);
write_cmos_sensor(0x6F12, 0x0446);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x92F9);
write_cmos_sensor(0x6F12, 0x2070);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x94F9);
write_cmos_sensor(0x6F12, 0x6070);
write_cmos_sensor(0x6F12, 0xB748);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0x2E06);
write_cmos_sensor(0x6F12, 0x28B3);
write_cmos_sensor(0x6F12, 0x0228);
write_cmos_sensor(0x6F12, 0x29D0);
write_cmos_sensor(0x6F12, 0x0120);
write_cmos_sensor(0x6F12, 0xB449);
write_cmos_sensor(0x6F12, 0x0123);
write_cmos_sensor(0x6F12, 0x91F8);
write_cmos_sensor(0x6F12, 0xC115);
write_cmos_sensor(0x6F12, 0x4143);
write_cmos_sensor(0x6F12, 0xC1F3);
write_cmos_sensor(0x6F12, 0x4700);
write_cmos_sensor(0x6F12, 0x2178);
write_cmos_sensor(0x6F12, 0xC840);
write_cmos_sensor(0x6F12, 0xAF49);
write_cmos_sensor(0x6F12, 0xA070);
write_cmos_sensor(0x6F12, 0x6278);
write_cmos_sensor(0x6F12, 0x91F8);
write_cmos_sensor(0x6F12, 0xC015);
write_cmos_sensor(0x6F12, 0x4908);
write_cmos_sensor(0x6F12, 0xD140);
write_cmos_sensor(0x6F12, 0x4200);
write_cmos_sensor(0x6F12, 0x03FA);
write_cmos_sensor(0x6F12, 0x02F0);
write_cmos_sensor(0x6F12, 0xE170);
write_cmos_sensor(0x6F12, 0x401E);
write_cmos_sensor(0x6F12, 0x2071);
write_cmos_sensor(0x6F12, 0x4900);
write_cmos_sensor(0x6F12, 0x03FA);
write_cmos_sensor(0x6F12, 0x01F0);
write_cmos_sensor(0x6F12, 0x401E);
write_cmos_sensor(0x6F12, 0x6071);
write_cmos_sensor(0x6F12, 0xA648);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0x2E06);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x01D1);
write_cmos_sensor(0x6F12, 0xA549);
write_cmos_sensor(0x6F12, 0xC880);
write_cmos_sensor(0x6F12, 0x10BD);
write_cmos_sensor(0x6F12, 0xA44A);
write_cmos_sensor(0x6F12, 0x40F2);
write_cmos_sensor(0x6F12, 0x5551);
write_cmos_sensor(0x6F12, 0x0120);
write_cmos_sensor(0x6F12, 0x1180);
write_cmos_sensor(0x6F12, 0xD6E7);
write_cmos_sensor(0x6F12, 0x9F48);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0x7A09);
write_cmos_sensor(0x6F12, 0xD2E7);
write_cmos_sensor(0x6F12, 0x010C);
write_cmos_sensor(0x6F12, 0xC900);
write_cmos_sensor(0x6F12, 0x80B2);
write_cmos_sensor(0x6F12, 0x4242);
write_cmos_sensor(0x6F12, 0x1040);
write_cmos_sensor(0x6F12, 0xB0FA);
write_cmos_sensor(0x6F12, 0x80F0);
write_cmos_sensor(0x6F12, 0xA1F5);
write_cmos_sensor(0x6F12, 0x4231);
write_cmos_sensor(0x6F12, 0xC0F1);
write_cmos_sensor(0x6F12, 0x1F00);
write_cmos_sensor(0x6F12, 0x0844);
write_cmos_sensor(0x6F12, 0x80B2);
write_cmos_sensor(0x6F12, 0x7047);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF047);
write_cmos_sensor(0x6F12, 0x9849);
write_cmos_sensor(0x6F12, 0x8146);
write_cmos_sensor(0x6F12, 0x0888);
write_cmos_sensor(0x6F12, 0x934A);
write_cmos_sensor(0x6F12, 0x92F8);
write_cmos_sensor(0x6F12, 0x7131);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x103A);
write_cmos_sensor(0x6F12, 0xB8EE);
write_cmos_sensor(0x6F12, 0x400A);
write_cmos_sensor(0x6F12, 0x9FED);
write_cmos_sensor(0x6F12, 0x941A);
write_cmos_sensor(0x6F12, 0x31EE);
write_cmos_sensor(0x6F12, 0x400A);
write_cmos_sensor(0x6F12, 0xC0EE);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x100A);
write_cmos_sensor(0x6F12, 0xB8EE);
write_cmos_sensor(0x6F12, 0x400A);
write_cmos_sensor(0x6F12, 0x20EE);
write_cmos_sensor(0x6F12, 0x800A);
write_cmos_sensor(0x6F12, 0xFCEE);
write_cmos_sensor(0x6F12, 0xC00A);
write_cmos_sensor(0x6F12, 0x10EE);
write_cmos_sensor(0x6F12, 0x900A);
write_cmos_sensor(0x6F12, 0x0880);
write_cmos_sensor(0x6F12, 0x8B1C);
write_cmos_sensor(0x6F12, 0x1888);
write_cmos_sensor(0x6F12, 0x92F8);
write_cmos_sensor(0x6F12, 0x7011);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x901A);
write_cmos_sensor(0x6F12, 0xF8EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0x71EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0xC0EE);
write_cmos_sensor(0x6F12, 0x811A);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x900A);
write_cmos_sensor(0x6F12, 0xF8EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0x61EE);
write_cmos_sensor(0x6F12, 0xA00A);
write_cmos_sensor(0x6F12, 0xF4EE);
write_cmos_sensor(0x6F12, 0xC00A);
write_cmos_sensor(0x6F12, 0xF1EE);
write_cmos_sensor(0x6F12, 0x10FA);
write_cmos_sensor(0x6F12, 0x01DC);
write_cmos_sensor(0x6F12, 0xF0EE);
write_cmos_sensor(0x6F12, 0x400A);
write_cmos_sensor(0x6F12, 0xFCEE);
write_cmos_sensor(0x6F12, 0xE00A);
write_cmos_sensor(0x6F12, 0x10EE);
write_cmos_sensor(0x6F12, 0x900A);
write_cmos_sensor(0x6F12, 0x1880);
write_cmos_sensor(0x6F12, 0x7C49);
write_cmos_sensor(0x6F12, 0x891E);
write_cmos_sensor(0x6F12, 0x0888);
write_cmos_sensor(0x6F12, 0x9246);
write_cmos_sensor(0x6F12, 0x92F8);
write_cmos_sensor(0x6F12, 0x7221);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x902A);
write_cmos_sensor(0x6F12, 0xF8EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0x71EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0xC0EE);
write_cmos_sensor(0x6F12, 0x811A);
write_cmos_sensor(0x6F12, 0x00EE);
write_cmos_sensor(0x6F12, 0x900A);
write_cmos_sensor(0x6F12, 0xF8EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0x61EE);
write_cmos_sensor(0x6F12, 0xA00A);
write_cmos_sensor(0x6F12, 0xF4EE);
write_cmos_sensor(0x6F12, 0xC00A);
write_cmos_sensor(0x6F12, 0xF1EE);
write_cmos_sensor(0x6F12, 0x10FA);
write_cmos_sensor(0x6F12, 0x01DD);
write_cmos_sensor(0x6F12, 0xB0EE);
write_cmos_sensor(0x6F12, 0x600A);
write_cmos_sensor(0x6F12, 0xBCEE);
write_cmos_sensor(0x6F12, 0xC00A);
write_cmos_sensor(0x6F12, 0x10EE);
write_cmos_sensor(0x6F12, 0x100A);
write_cmos_sensor(0x6F12, 0x0880);
write_cmos_sensor(0x6F12, 0x6E48);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0x2C11);
write_cmos_sensor(0x6F12, 0x0229);
write_cmos_sensor(0x6F12, 0x01D1);
write_cmos_sensor(0x6F12, 0xB0F8);
write_cmos_sensor(0x6F12, 0x2E91);
write_cmos_sensor(0x6F12, 0x6B4E);
write_cmos_sensor(0x6F12, 0xDFF8);
write_cmos_sensor(0x6F12, 0xB081);
write_cmos_sensor(0x6F12, 0x0024);
write_cmos_sensor(0x6F12, 0x4246);
write_cmos_sensor(0x6F12, 0x18F8);
write_cmos_sensor(0x6F12, 0x0410);
write_cmos_sensor(0x6F12, 0xC9B3);
write_cmos_sensor(0x6F12, 0x4FEA);
write_cmos_sensor(0x6F12, 0xD900);
write_cmos_sensor(0x6F12, 0x0229);
write_cmos_sensor(0x6F12, 0x03D1);
write_cmos_sensor(0x6F12, 0xD6F8);
write_cmos_sensor(0x6F12, 0xFC13);
write_cmos_sensor(0x6F12, 0x4843);
write_cmos_sensor(0x6F12, 0x400B);
write_cmos_sensor(0x6F12, 0x02EB);
write_cmos_sensor(0x6F12, 0x4401);
write_cmos_sensor(0x6F12, 0x4B88);
write_cmos_sensor(0x6F12, 0x8342);
write_cmos_sensor(0x6F12, 0x01D3);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x03E0);
write_cmos_sensor(0x6F12, 0xC988);
write_cmos_sensor(0x6F12, 0x8142);
write_cmos_sensor(0x6F12, 0x03D8);
write_cmos_sensor(0x6F12, 0x0220);
write_cmos_sensor(0x6F12, 0x3119);
write_cmos_sensor(0x6F12, 0x81F8);
write_cmos_sensor(0x6F12, 0x940A);
write_cmos_sensor(0x6F12, 0x04EB);
write_cmos_sensor(0x6F12, 0x4400);
write_cmos_sensor(0x6F12, 0x0025);
write_cmos_sensor(0x6F12, 0x08EB);
write_cmos_sensor(0x6F12, 0x0017);
write_cmos_sensor(0x6F12, 0x16E0);
write_cmos_sensor(0x6028, 0x2002);
write_cmos_sensor(0x602A, 0x0000);
write_cmos_sensor(0x6F12, 0x04EB);
write_cmos_sensor(0x6F12, 0x4403);
write_cmos_sensor(0x6F12, 0x1B01);
write_cmos_sensor(0x6F12, 0x03EB);
write_cmos_sensor(0x6F12, 0x4101);
write_cmos_sensor(0x6F12, 0x3319);
write_cmos_sensor(0x6F12, 0x93F8);
write_cmos_sensor(0x6F12, 0x943A);
write_cmos_sensor(0x6F12, 0x03EB);
write_cmos_sensor(0x6F12, 0x0802);
write_cmos_sensor(0x6F12, 0x0A44);
write_cmos_sensor(0x6F12, 0x0AEB);
write_cmos_sensor(0x6F12, 0x0411);
write_cmos_sensor(0x6F12, 0x01EB);
write_cmos_sensor(0x6F12, 0x4501);
write_cmos_sensor(0x6F12, 0x5289);
write_cmos_sensor(0x6F12, 0xB1F8);
write_cmos_sensor(0x6F12, 0x4E11);
write_cmos_sensor(0x6F12, 0xFFF7);
write_cmos_sensor(0x6F12, 0xD5FA);
write_cmos_sensor(0x6F12, 0x6D1C);
write_cmos_sensor(0x6F12, 0x082D);
write_cmos_sensor(0x6F12, 0x06D2);
write_cmos_sensor(0x6F12, 0x05EB);
write_cmos_sensor(0x6F12, 0x4501);
write_cmos_sensor(0x6F12, 0x07EB);
write_cmos_sensor(0x6F12, 0x4100);
write_cmos_sensor(0x6F12, 0xC089);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0xE1D1);
write_cmos_sensor(0x6F12, 0x641C);
write_cmos_sensor(0x6F12, 0x022C);
write_cmos_sensor(0x6F12, 0xBED3);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xF087);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF74F);
write_cmos_sensor(0x6F12, 0x8B46);
write_cmos_sensor(0x6F12, 0x4849);
write_cmos_sensor(0x6F12, 0x49F2);
write_cmos_sensor(0x6F12, 0x0D00);
write_cmos_sensor(0x6F12, 0x1446);
write_cmos_sensor(0x6F12, 0xA1F8);
write_cmos_sensor(0x6F12, 0x6200);
write_cmos_sensor(0x6F12, 0x5946);
write_cmos_sensor(0x6F12, 0x0098);
write_cmos_sensor(0x6F12, 0xFDF7);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x9BF8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0228);
write_cmos_sensor(0x6F12, 0x6ED1);
write_cmos_sensor(0x6F12, 0x424A);
write_cmos_sensor(0x6F12, 0x4FF0);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x4FF4);
write_cmos_sensor(0x6F12, 0x805E);
write_cmos_sensor(0x6F12, 0x82F8);
write_cmos_sensor(0x6F12, 0x03A1);
write_cmos_sensor(0x6F12, 0xC4F8);
write_cmos_sensor(0x6F12, 0x0CE0);
write_cmos_sensor(0x6F12, 0x6088);
write_cmos_sensor(0x6F12, 0xE188);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0xB0FB);
write_cmos_sensor(0x6F12, 0xF1F0);
write_cmos_sensor(0x6F12, 0x3149);
write_cmos_sensor(0x6F12, 0x85B2);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0xB1F8);
write_cmos_sensor(0x6F12, 0x6E11);
write_cmos_sensor(0x6F12, 0x9146);
write_cmos_sensor(0x6F12, 0x0E03);
write_cmos_sensor(0x6F12, 0x99F8);
write_cmos_sensor(0x6F12, 0x0411);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x4008);
write_cmos_sensor(0x6F12, 0x01EB);
write_cmos_sensor(0x6F12, 0x4101);
write_cmos_sensor(0x6F12, 0x09EB);
write_cmos_sensor(0x6F12, 0x0111);
write_cmos_sensor(0x6F12, 0x01EB);
write_cmos_sensor(0x6F12, 0x8803);
write_cmos_sensor(0x6F12, 0xB3F8);
write_cmos_sensor(0x6F12, 0x0CC1);
write_cmos_sensor(0x6F12, 0xB3F8);
write_cmos_sensor(0x6F12, 0x0811);
write_cmos_sensor(0x6F12, 0xB6FB);
write_cmos_sensor(0x6F12, 0xFCFC);
write_cmos_sensor(0x6F12, 0xB3F8);
write_cmos_sensor(0x6F12, 0x0A21);
write_cmos_sensor(0x6F12, 0xC3F8);
write_cmos_sensor(0x6F12, 0x10C1);
write_cmos_sensor(0x6F12, 0x09EB);
write_cmos_sensor(0x6F12, 0x400C);
write_cmos_sensor(0x6F12, 0xBCF8);
write_cmos_sensor(0x6F12, 0x9030);
write_cmos_sensor(0x6F12, 0x03B1);
write_cmos_sensor(0x6F12, 0x1946);
write_cmos_sensor(0x6F12, 0xBCF8);
write_cmos_sensor(0x6F12, 0x9830);
write_cmos_sensor(0x6F12, 0x03B1);
write_cmos_sensor(0x6F12, 0x1A46);
write_cmos_sensor(0x6F12, 0x0B02);
write_cmos_sensor(0x6F12, 0xB3FB);
write_cmos_sensor(0x6F12, 0xF2F3);
write_cmos_sensor(0x6F12, 0x9BB2);
write_cmos_sensor(0x6F12, 0x09EB);
write_cmos_sensor(0x6F12, 0x8808);
write_cmos_sensor(0x6F12, 0xB6FB);
write_cmos_sensor(0x6F12, 0xF3F7);
write_cmos_sensor(0x6F12, 0xA8F8);
write_cmos_sensor(0x6F12, 0xC811);
write_cmos_sensor(0x6F12, 0x09EB);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0xA8F8);
write_cmos_sensor(0x6F12, 0xCA21);
write_cmos_sensor(0x6F12, 0xA8F8);
write_cmos_sensor(0x6F12, 0xCC31);
write_cmos_sensor(0x6F12, 0x401C);
write_cmos_sensor(0x6F12, 0xC1F8);
write_cmos_sensor(0x6F12, 0xF871);
write_cmos_sensor(0x6F12, 0x0428);
write_cmos_sensor(0x6F12, 0xCCDB);
write_cmos_sensor(0x6F12, 0x7146);
write_cmos_sensor(0x6F12, 0x8D42);
write_cmos_sensor(0x6F12, 0x04D9);
write_cmos_sensor(0x6F12, 0x89F8);
write_cmos_sensor(0x6F12, 0x02A1);
write_cmos_sensor(0x6F12, 0xA560);
write_cmos_sensor(0x6F12, 0x7546);
write_cmos_sensor(0x6F12, 0x03E0);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0x89F8);
write_cmos_sensor(0x6F12, 0x0221);
write_cmos_sensor(0x6F12, 0xA160);
write_cmos_sensor(0x6F12, 0x009A);
write_cmos_sensor(0x6F12, 0xE808);
write_cmos_sensor(0x6F12, 0x1060);
write_cmos_sensor(0x6F12, 0x134A);
write_cmos_sensor(0x6F12, 0xA2F8);
write_cmos_sensor(0x6F12, 0xD201);
write_cmos_sensor(0x6F12, 0x9BF8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x2070);
write_cmos_sensor(0x6F12, 0x0098);
write_cmos_sensor(0x6F12, 0x4FF6);
write_cmos_sensor(0x6F12, 0xFF72);
write_cmos_sensor(0x6F12, 0x0068);
write_cmos_sensor(0x6F12, 0x02EA);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0x6F12, 0xA080);
write_cmos_sensor(0x6F12, 0x2A03);
write_cmos_sensor(0x6F12, 0xB2FB);
write_cmos_sensor(0x6F12, 0xF0F0);
write_cmos_sensor(0x6F12, 0x8842);
write_cmos_sensor(0x6F12, 0x05D0);
write_cmos_sensor(0x6F12, 0x89F8);
write_cmos_sensor(0x6F12, 0x02A1);
write_cmos_sensor(0x6F12, 0xA168);
write_cmos_sensor(0x6F12, 0x4143);
write_cmos_sensor(0x6F12, 0x080B);
write_cmos_sensor(0x6F12, 0xA060);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xFE8F);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFC00);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x8810);
write_cmos_sensor(0x6F12, 0x4001);
write_cmos_sensor(0x6F12, 0xB000);
write_cmos_sensor(0x6F12, 0x4001);
write_cmos_sensor(0x6F12, 0xBC06);
write_cmos_sensor(0x6F12, 0x4001);
write_cmos_sensor(0x6F12, 0x991C);
write_cmos_sensor(0x6F12, 0x42C8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x8070);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x4C00);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x56A0);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xF800);
write_cmos_sensor(0x6F12, 0x40F6);
write_cmos_sensor(0x6F12, 0x0B2C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x4DF2);
write_cmos_sensor(0x6F12, 0x0F2C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x4DF2);
write_cmos_sensor(0x6F12, 0x973C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x4DF2);
write_cmos_sensor(0x6F12, 0x8F3C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF84A);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0xF88C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF890);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x22FE);
write_cmos_sensor(0x6F12, 0x0303);
write_cmos_sensor(0x602A, 0x37C4);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0008);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x1FF4);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x4FD6);
write_cmos_sensor(0x6F12, 0x060D);
write_cmos_sensor(0x0D80, 0x1388);
write_cmos_sensor(0x602A, 0x1336);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x602A, 0x4FA0);
write_cmos_sensor(0x6F12, 0x0105);
write_cmos_sensor(0x6F12, 0x080A);
write_cmos_sensor(0x6F12, 0xFF04);
write_cmos_sensor(0x6F12, 0x00FF);
write_cmos_sensor(0x602A, 0x4FA8);
write_cmos_sensor(0x6F12, 0x0704);
write_cmos_sensor(0x6F12, 0x0C00);
write_cmos_sensor(0x6F12, 0x0700);
write_cmos_sensor(0x602A, 0x4FAE);
write_cmos_sensor(0x6F12, 0x0400);
write_cmos_sensor(0x602A, 0x4FBA);
write_cmos_sensor(0x6F12, 0x2640);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0170);
write_cmos_sensor(0x6F12, 0x001E);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0037);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x602A, 0x5040);
write_cmos_sensor(0x6F12, 0x06D0);
write_cmos_sensor(0x6F12, 0x1388);
write_cmos_sensor(0x602A, 0x5010);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x036D);
write_cmos_sensor(0x602A, 0x4FE6);
write_cmos_sensor(0x6F12, 0x0505);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x0008);
write_cmos_sensor(0x6F12, 0x0707);
write_cmos_sensor(0x6F12, 0x0606);
write_cmos_sensor(0x6F12, 0x0504);
write_cmos_sensor(0x6F12, 0x0301);
write_cmos_sensor(0x6F12, 0x050B);
write_cmos_sensor(0x6F12, 0x0D0F);
write_cmos_sensor(0x6F12, 0x1216);
write_cmos_sensor(0x6F12, 0x1718);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x602A, 0x0B2E);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x0FEA);
write_cmos_sensor(0x6F12, 0x2440);
#else
	write_cmos_sensor(0xFCFC, 0x4000);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x0000, 0x010C);
write_cmos_sensor(0x0000, 0x0971);
write_cmos_sensor(0x6010, 0x0001);
mdelay(30);     
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x0A02, 0x7E00);
write_cmos_sensor(0x6028, 0x2001); 
write_cmos_sensor(0x602A, 0xFB74);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0C48);
write_cmos_sensor(0x6F12, 0x10B5);
write_cmos_sensor(0x6F12, 0x0A49);
write_cmos_sensor(0x6F12, 0x0164);
write_cmos_sensor(0x6F12, 0x0B49);
write_cmos_sensor(0x6F12, 0x8160);
write_cmos_sensor(0x6F12, 0x0B49);
write_cmos_sensor(0x6F12, 0x8163);
write_cmos_sensor(0x6F12, 0x436C);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0x0A49);
write_cmos_sensor(0x6F12, 0x0B48);
write_cmos_sensor(0x6F12, 0x9847);
write_cmos_sensor(0x6F12, 0x0B49);
write_cmos_sensor(0x6F12, 0xC863);
write_cmos_sensor(0x6F12, 0x0B49);
write_cmos_sensor(0x6F12, 0x43F2);
write_cmos_sensor(0x6F12, 0xC170);
write_cmos_sensor(0x6F12, 0xC1F8);
write_cmos_sensor(0x6F12, 0xC407);
write_cmos_sensor(0x6F12, 0x0949);
write_cmos_sensor(0x6F12, 0x0968);
write_cmos_sensor(0x6F12, 0x4883);
write_cmos_sensor(0x6F12, 0x10BD);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFD11);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x9B20);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFD1D);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFD29);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFC05);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0xC4C5);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xF780);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x4C00);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0C60);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x30B5);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x039C);
write_cmos_sensor(0x6F12, 0x33F8);
write_cmos_sensor(0x6F12, 0x1050);
write_cmos_sensor(0x6F12, 0x01F1);
write_cmos_sensor(0x6F12, 0x8042);
write_cmos_sensor(0x6F12, 0x2540);
write_cmos_sensor(0x6F12, 0x1580);
write_cmos_sensor(0x6F12, 0x33F8);
write_cmos_sensor(0x6F12, 0x1050);
write_cmos_sensor(0x6F12, 0x2540);
write_cmos_sensor(0x6F12, 0x5581);
write_cmos_sensor(0x6F12, 0x401C);
write_cmos_sensor(0x6F12, 0x891C);
write_cmos_sensor(0x6F12, 0x0528);
write_cmos_sensor(0x6F12, 0xF1DB);
write_cmos_sensor(0x6F12, 0x30BD);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF843);
write_cmos_sensor(0x6F12, 0x5848);
write_cmos_sensor(0x6F12, 0x0022);
write_cmos_sensor(0x6F12, 0xC16B);
write_cmos_sensor(0x6F12, 0x0C0C);
write_cmos_sensor(0x6F12, 0x8DB2);
write_cmos_sensor(0x6F12, 0x2946);
write_cmos_sensor(0x6F12, 0x2046);
write_cmos_sensor(0x6F12, 0xFFF7);
write_cmos_sensor(0x6F12, 0x05FC);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0xB7F8);
write_cmos_sensor(0x6F12, 0x0122);
write_cmos_sensor(0x6F12, 0x2946);
write_cmos_sensor(0x6F12, 0x2046);
write_cmos_sensor(0x6F12, 0xFFF7);
write_cmos_sensor(0x6F12, 0xFEFB);
write_cmos_sensor(0x6F12, 0x5148);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0xC500);
write_cmos_sensor(0x6F12, 0x0228);
write_cmos_sensor(0x6F12, 0x33D1);
write_cmos_sensor(0x6F12, 0xDFF8);
write_cmos_sensor(0x6F12, 0x40C1);
write_cmos_sensor(0x6F12, 0x504F);
write_cmos_sensor(0x6F12, 0x504E);
write_cmos_sensor(0x6F12, 0x9CF8);
write_cmos_sensor(0x6F12, 0x0F02);
write_cmos_sensor(0x6F12, 0x20B3);
write_cmos_sensor(0x6F12, 0x4F48);
write_cmos_sensor(0x6F12, 0xD0F8);
write_cmos_sensor(0x6F12, 0xC091);
write_cmos_sensor(0x6F12, 0x0120);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x1201);
write_cmos_sensor(0x6F12, 0x4D4D);
write_cmos_sensor(0x6F12, 0x0024);
write_cmos_sensor(0x6F12, 0x43F6);
write_cmos_sensor(0x6F12, 0xFF78);
write_cmos_sensor(0x6F12, 0x04EB);
write_cmos_sensor(0x6F12, 0x8400);
write_cmos_sensor(0x6F12, 0x07EB);
write_cmos_sensor(0x6F12, 0x4003);
write_cmos_sensor(0x6F12, 0x03F5);
write_cmos_sensor(0x6F12, 0xE973);
write_cmos_sensor(0x6F12, 0x0522);
write_cmos_sensor(0x6F12, 0x2946);
write_cmos_sensor(0x6F12, 0x4846);
write_cmos_sensor(0x6F12, 0xCDF8);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0xFFF7);
write_cmos_sensor(0x6F12, 0xB9FF);
write_cmos_sensor(0x6F12, 0x1435);
write_cmos_sensor(0x6F12, 0x641C);
write_cmos_sensor(0x6F12, 0x062C);
write_cmos_sensor(0x6F12, 0xEED3);
write_cmos_sensor(0x6F12, 0x41F6);
write_cmos_sensor(0x6F12, 0xFF70);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x3002);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x3A02);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x4402);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x4E02);
write_cmos_sensor(0x6F12, 0x9CF8);
write_cmos_sensor(0x6F12, 0x0F02);
write_cmos_sensor(0x6F12, 0x07EB);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x6F12, 0xB0F8);
write_cmos_sensor(0x6F12, 0x0E02);
write_cmos_sensor(0x6F12, 0xA6F8);
write_cmos_sensor(0x6F12, 0x1001);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xF883);
write_cmos_sensor(0x6F12, 0x3A48);
write_cmos_sensor(0x6F12, 0x0088);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x14D0);
write_cmos_sensor(0x6F12, 0x344A);
write_cmos_sensor(0x6F12, 0x1078);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x10D0);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x09E0);
write_cmos_sensor(0x6F12, 0x01F1);
write_cmos_sensor(0x6F12, 0x8041);
write_cmos_sensor(0x6F12, 0x0988);
write_cmos_sensor(0x6F12, 0x02EB);
write_cmos_sensor(0x6F12, 0x4003);
write_cmos_sensor(0x6F12, 0x401C);
write_cmos_sensor(0x6F12, 0xA3F8);
write_cmos_sensor(0x6F12, 0x1812);
write_cmos_sensor(0x6F12, 0x2028);
write_cmos_sensor(0x6F12, 0x04D2);
write_cmos_sensor(0x6F12, 0x02EB);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x4968);
write_cmos_sensor(0x6F12, 0x0029);
write_cmos_sensor(0x6F12, 0xF0D1);
write_cmos_sensor(0x6F12, 0x7047);
write_cmos_sensor(0x6F12, 0x10B5);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x5FF8);
write_cmos_sensor(0x6F12, 0x0128);
write_cmos_sensor(0x6F12, 0x18D1);
write_cmos_sensor(0x6F12, 0x2A48);
write_cmos_sensor(0x6F12, 0x0088);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x14D0);
write_cmos_sensor(0x6F12, 0x244A);
write_cmos_sensor(0x6F12, 0x1078);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x10D0);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x09E0);
write_cmos_sensor(0x6F12, 0x02EB);
write_cmos_sensor(0x6F12, 0x4003);
write_cmos_sensor(0x6F12, 0x01F1);
write_cmos_sensor(0x6F12, 0x8041);
write_cmos_sensor(0x6F12, 0xB3F8);
write_cmos_sensor(0x6F12, 0x1832);
write_cmos_sensor(0x6F12, 0x0B80);
write_cmos_sensor(0x6F12, 0x401C);
write_cmos_sensor(0x6F12, 0x2028);
write_cmos_sensor(0x6F12, 0x04D2);
write_cmos_sensor(0x6F12, 0x02EB);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x4968);
write_cmos_sensor(0x6F12, 0x0029);
write_cmos_sensor(0x6F12, 0xF0D1);
write_cmos_sensor(0x6F12, 0x10BD);
write_cmos_sensor(0x6F12, 0x10B5);
write_cmos_sensor(0x6F12, 0xFEF7);
write_cmos_sensor(0x6F12, 0x9EF8);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0x1040);
write_cmos_sensor(0x6F12, 0xDAE7);
write_cmos_sensor(0x6F12, 0x10B5);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x3FF8);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0x1040);
write_cmos_sensor(0x6F12, 0xBAE7);
write_cmos_sensor(0x6F12, 0x2DE9);
write_cmos_sensor(0x6F12, 0xF041);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x3DF8);
write_cmos_sensor(0x6F12, 0x114D);
write_cmos_sensor(0x6F12, 0x4FF2);
write_cmos_sensor(0x6F12, 0x5E54);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x6818);
write_cmos_sensor(0x6F12, 0x90F8);
write_cmos_sensor(0x6F12, 0x8400);
write_cmos_sensor(0x6F12, 0x78B1);
write_cmos_sensor(0x6F12, 0x01EB);
write_cmos_sensor(0x6F12, 0xC102);
write_cmos_sensor(0x6F12, 0x05EB);
write_cmos_sensor(0x6F12, 0x8202);
write_cmos_sensor(0x6F12, 0x8E32);
write_cmos_sensor(0x6F12, 0x2046);
write_cmos_sensor(0x6F12, 0x0023);
write_cmos_sensor(0x6F12, 0x4FF0);
write_cmos_sensor(0x6F12, 0x8046);
write_cmos_sensor(0x6F12, 0x32F8);
write_cmos_sensor(0x6F12, 0x027B);
write_cmos_sensor(0x6F12, 0x3752);
write_cmos_sensor(0x6F12, 0x5B1C);
write_cmos_sensor(0x6F12, 0x801C);
write_cmos_sensor(0x6F12, 0x122B);
write_cmos_sensor(0x6F12, 0xF6D3);
write_cmos_sensor(0x6F12, 0x2434);
write_cmos_sensor(0x6F12, 0x491C);
write_cmos_sensor(0x6F12, 0x0929);
write_cmos_sensor(0x6F12, 0xE7D3);
write_cmos_sensor(0x6F12, 0xBDE8);
write_cmos_sensor(0x6F12, 0xF081);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xF780);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0x8070);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xF800);
write_cmos_sensor(0x6F12, 0x2001);
write_cmos_sensor(0x6F12, 0xFDC0);
write_cmos_sensor(0x6F12, 0x4001);
write_cmos_sensor(0x6F12, 0x8000);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x9C10);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x81B4);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x8B70);
write_cmos_sensor(0x6F12, 0x4CF2);
write_cmos_sensor(0x6F12, 0xC54C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x010C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x40F6);
write_cmos_sensor(0x6F12, 0x0B2C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x4CF2);
write_cmos_sensor(0x6F12, 0x1B6C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x010C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x6F12, 0x4DF2);
write_cmos_sensor(0x6F12, 0x0F2C);
write_cmos_sensor(0x6F12, 0xC0F2);
write_cmos_sensor(0x6F12, 0x020C);
write_cmos_sensor(0x6F12, 0x6047);
write_cmos_sensor(0x602A, 0xF84A);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0xF88C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF890);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x22FE);
write_cmos_sensor(0x6F12, 0x0303);
write_cmos_sensor(0x602A, 0x37C2);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0008);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x1FF4);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x4FD6);
write_cmos_sensor(0x6F12, 0x060D);
write_cmos_sensor(0x0D80, 0x1388);
write_cmos_sensor(0x602A, 0x1336);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x602A, 0x4FA0);
write_cmos_sensor(0x6F12, 0x0105);
write_cmos_sensor(0x6F12, 0x080A);
write_cmos_sensor(0x6F12, 0xFF04);
write_cmos_sensor(0x6F12, 0x00FF);
write_cmos_sensor(0x602A, 0x4FA8);
write_cmos_sensor(0x6F12, 0x0704);
write_cmos_sensor(0x6F12, 0x0C00);
write_cmos_sensor(0x6F12, 0x0700);
write_cmos_sensor(0x602A, 0x4FAE);
write_cmos_sensor(0x6F12, 0x0400);
write_cmos_sensor(0x602A, 0x4FBA);
write_cmos_sensor(0x6F12, 0x2640);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0170);
write_cmos_sensor(0x6F12, 0x001E);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0037);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x602A, 0x5040);
write_cmos_sensor(0x6F12, 0x06D0);
write_cmos_sensor(0x6F12, 0x1388);
write_cmos_sensor(0x602A, 0x5010);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x036D);
write_cmos_sensor(0x602A, 0x4FE6);
write_cmos_sensor(0x6F12, 0x0505);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x0008);
write_cmos_sensor(0x6F12, 0x0707);
write_cmos_sensor(0x6F12, 0x0606);
write_cmos_sensor(0x6F12, 0x0504);
write_cmos_sensor(0x6F12, 0x0301);
write_cmos_sensor(0x6F12, 0x050B);
write_cmos_sensor(0x6F12, 0x0D0F);
write_cmos_sensor(0x6F12, 0x1216);
write_cmos_sensor(0x6F12, 0x1718);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x602A, 0x0B2E);
write_cmos_sensor(0x6F12, 0x0001);
#endif
	}	/*	sensor_init  */
static void preview_setting(void)
{
	/*	ZTE D2A2 for PDAF tail mode 2bin D2A2 2000x1500 30fps PDAF correc always on   */
  LOG_INF("E\n");
  #if 0
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x62C4);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x44DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x0007);
write_cmos_sensor(0xF448, 0x0000);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x002D);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFC00);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x602A, 0xFD4E);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x2000);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x2002);
write_cmos_sensor(0x6F12, 0x00BF);
write_cmos_sensor(0x602A, 0x2006);
write_cmos_sensor(0x6F12, 0x00C0);
write_cmos_sensor(0x602A, 0x200A);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x200C);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x602A, 0x200E);
write_cmos_sensor(0x6F12, 0xF402);
write_cmos_sensor(0x602A, 0x2010);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2012);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x602A, 0x2014);
write_cmos_sensor(0x6F12, 0xF404);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x4040);
write_cmos_sensor(0x0340, 0x0F32);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0000);
write_cmos_sensor(0x0310, 0x00C0);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0000);//czy chg
write_cmos_sensor(0x0D02, 0x0101);//Tail Mode ON 0x0101 0x0000
write_cmos_sensor(0x0114, 0x0301);//Tail Mode ON 0x0301 0x0300
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0BC8, 0x0001);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x37C2);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xFD6E);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8C2);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFD70);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x87BE);
write_cmos_sensor(0x6F12, 0x00D0);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x00D1);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x00D2);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x602A, 0x3DD4);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x8342);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x8352);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x602A, 0x8212);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x8222);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x602A, 0x8452);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x602A, 0x8462);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x602A, 0x8572);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x602A, 0x8582);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0340);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0180);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0301);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x591E);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x602A, 0x42AE);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x4966);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36CE);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6F12, 0x000E);
//iqv
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);                                  
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#else
	write_cmos_sensor(0x6028, 0x4000); 
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x04DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x023F);
write_cmos_sensor(0xF448, 0x00A1);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x0025);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFE44);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x83B0);
write_cmos_sensor(0x0340, 0x0DFE);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0002);
write_cmos_sensor(0x0310, 0x012C);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0100);
write_cmos_sensor(0x0D02, 0x0000);
write_cmos_sensor(0x0114, 0x0300);
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0FEA, 0x2440);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x0202, 0x0010);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DC);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0300);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0080);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x081E, 0x0A00);
write_cmos_sensor(0x6B5C, 0xE222);
write_cmos_sensor(0x6B5E, 0x00FF);
write_cmos_sensor(0x6B76, 0xE000);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#endif
}	/*	preview_setting  */
static void capture_setting(kal_uint16 currefps)
{
LOG_INF("E\n");
#if 0
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x62C4);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x44DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x0007);
write_cmos_sensor(0xF448, 0x0000);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x002D);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFC00);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x602A, 0xFD4E);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x2000);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x2002);
write_cmos_sensor(0x6F12, 0x00BF);
write_cmos_sensor(0x602A, 0x2006);
write_cmos_sensor(0x6F12, 0x00C0);
write_cmos_sensor(0x602A, 0x200A);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x200C);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x602A, 0x200E);
write_cmos_sensor(0x6F12, 0xF402);
write_cmos_sensor(0x602A, 0x2010);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2012);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x602A, 0x2014);
write_cmos_sensor(0x6F12, 0xF404);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x4040);
write_cmos_sensor(0x0340, 0x0F32);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0000);
write_cmos_sensor(0x0310, 0x00C0);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0000);//czy chg
write_cmos_sensor(0x0D02, 0x0101);//Tail Mode ON 0x0101 0x0000
write_cmos_sensor(0x0114, 0x0301);//Tail Mode ON 0x0301 0x0300
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0BC8, 0x0001);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x37C2);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xFD6E);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8C2);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFD70);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x87BE);
write_cmos_sensor(0x6F12, 0x00D0);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x00D1);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x00D2);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x602A, 0x3DD4);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x8342);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x8352);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x602A, 0x8212);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x8222);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x602A, 0x8452);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x602A, 0x8462);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x602A, 0x8572);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x602A, 0x8582);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0340);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0180);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0301);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x591E);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x602A, 0x42AE);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x4966);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36CE);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6F12, 0x000E);
//iqv
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);                                  
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#else
	write_cmos_sensor(0x6028, 0x4000); 
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x04DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x023F);
write_cmos_sensor(0xF448, 0x00A1);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x0025);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFE44);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x83B0);
write_cmos_sensor(0x0340, 0x0DFE);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0002);
write_cmos_sensor(0x0310, 0x012C);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0100);
write_cmos_sensor(0x0D02, 0x0000);
write_cmos_sensor(0x0114, 0x0300);
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0FEA, 0x2440);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x0202, 0x0010);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DC);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0300);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0080);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x081E, 0x0A00);
write_cmos_sensor(0x6B5C, 0xE222);
write_cmos_sensor(0x6B5E, 0x00FF);
write_cmos_sensor(0x6B76, 0xE000);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#endif
 } /* capture setting */

#if 0
static kal_uint16 s5kgw1_normal_video_setting[] = {
	0x6028, 0x4000,
	0x6214, 0xF9F0,
	0x6218, 0xF150,
	0x6028, 0x2000,
	0x602A, 0x36AA,
	0x6F12, 0x0060,
	0x6F12, 0x0061,
	0x6F12, 0x0070,
	0x6F12, 0x0071,
	0x602A, 0x36CA,
	0x6F12, 0x1FFF,
	0x6F12, 0x1FFF,
	0x6A12, 0x0000,
	0x602A, 0x2090,
	0x6F12, 0xFFFF,
	0x602A, 0x62C4,
	0x6F12, 0x0100,
	0x6F12, 0x0000,
	0x602A, 0x3670,
	0x6F12, 0x0505,
	0x6F12, 0x0505,
	0x602A, 0x368C,
	0x6F12, 0x387F,
	0x6F12, 0xFFFC,
	0x602A, 0x37B0,
	0x6F12, 0x000A,
	0x6F12, 0x0018,
	0x6F12, 0x0014,
	0x6F12, 0x0013,
	0x602A, 0x37BA,
	0x6F12, 0x0011,
	0x602A, 0x3680,
	0x6F12, 0x0000,
	0x6F12, 0x0280,
	0x6F12, 0x5008,
	0x602A, 0x235C,
	0x6F12, 0x0009,
	0x602A, 0x37A8,
	0x6F12, 0x0000,
	0x602A, 0x36D6,
	0x6F12, 0x44DE,
	0x6F12, 0x80AE,
	0x6F12, 0xC000,
	0xF446, 0x0007,
	0xF448, 0x0000,
	0x602A, 0x37A0,
	0x6F12, 0x0000,
	0x602A, 0x2594,
	0x6F12, 0x002B,
	0x602A, 0x259C,
	0x6F12, 0x05EA,
	0x602A, 0x25A4,
	0x6F12, 0x05FF,
	0x602A, 0x25AC,
	0x6F12, 0x05FF,
	0x602A, 0x25B4,
	0x6F12, 0x002B,
	0x602A, 0x25BC,
	0x6F12, 0x05EA,
	0x602A, 0x25C4,
	0x6F12, 0x05FF,
	0x602A, 0x25CC,
	0x6F12, 0x05FF,
	0x602A, 0x3690,
	0x6F12, 0xFFEA,
	0x602A, 0x285C,
	0x6F12, 0x0101,
	0x602A, 0x286C,
	0x6F12, 0x0234,
	0x602A, 0x2474,
	0x6F12, 0x0110,
	0x602A, 0x247C,
	0x6F12, 0x00F8,
	0x602A, 0x3694,
	0x6F12, 0x000F,
	0x602A, 0x2C62,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x602A, 0x236C,
	0x6F12, 0x0009,
	0x602A, 0x249C,
	0x6F12, 0x000A,
	0x602A, 0x3686,
	0x6F12, 0x0808,
	0x602A, 0x37A2,
	0x6F12, 0x002D,
	0x602A, 0x1388,
	0x6F12, 0x0080,
	0x602A, 0x2124,
	0x6F12, 0x0100,
	0x6F12, 0x1F40,
	0x602A, 0x3EA4,
	0x6F12, 0x0003,
	0x602A, 0x3E00,
	0x6F12, 0x0201,
	0x602A, 0x2336,
	0x6F12, 0xFFFE,
	0x602A, 0x3678,
	0x6F12, 0x0707,
	0x602A, 0x2344,
	0x6F12, 0x0023,
	0x602A, 0x2354,
	0x6F12, 0x008C,
	0x602A, 0x2384,
	0x6F12, 0x0090,
	0x602A, 0x2464,
	0x6F12, 0x0012,
	0x602A, 0x248C,
	0x6F12, 0x0011,
	0x6028, 0x2001,
	0x602A, 0x4F4C,
	0x6F12, 0x0000,
	0x602A, 0xFC00,
	0x6F12, 0x0101,
	0x6F12, 0x0101,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x00A3,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0032,
	0x6F12, 0x0000,
	0x602A, 0xFD4E,
	0x6F12, 0x4000,
	0x6F12, 0x0003,
	0x6028, 0x2000,
	0x602A, 0x2000,
	0x6F12, 0x0100,
	0x602A, 0x2002,
	0x6F12, 0x00BF,
	0x602A, 0x2006,
	0x6F12, 0x00C0,
	0x602A, 0x200A,
	0x6F12, 0x0000,
	0x602A, 0x200C,
	0x6F12, 0x4000,
	0x602A, 0x200E,
	0x6F12, 0xF402,
	0x602A, 0x2010,
	0x6F12, 0x0000,
	0x602A, 0x2012,
	0x6F12, 0x0003,
	0x602A, 0x2014,
	0x6F12, 0xF404,
	0x0110, 0x1002,
	0x011C, 0x0100,
	0x0344, 0x0320,
	0x0346, 0x0520,
	0x0348, 0x211F,
	0x034A, 0x15FF,
	0x034C, 0x0780,
	0x034E, 0x0438,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0342, 0x5200,
	0x0340, 0x0BE0,
	0x602A, 0x3652,
	0x6F12, 0x0401,
	0x0380, 0x0002,
	0x0382, 0x0006,
	0x0384, 0x0002,
	0x0386, 0x0006,
	0x0900, 0x3344,
	0x040C, 0x0000,
	0x0404, 0x1000,
	0x0408, 0x0100,
	0x040A, 0x0100,
	0x0400, 0x1010,
	0x0136, 0x1800,
	0x0304, 0x0004,
	0x030C, 0x0000,
	0x0306, 0x00A0,
	0x0302, 0x0001,
	0x0300, 0x0004,
	0x030E, 0x0004,
	0x0312, 0x0003,
	0x0310, 0x014E,
	0x602A, 0x368A,
	0x6F12, 0x0096,
	0x021E, 0x0000,
	0x0D00, 0x0000,
	0x0D02, 0x0000,
	0x0114, 0x0300,
	0x602A, 0x593C,
	0x6F12, 0x0104,
	0x6F12, 0x0802,
	0x6F12, 0x0100,
	0x6F12, 0x0141,
	0x6F12, 0x0182,
	0x6F12, 0x03C3,
	0x6F12, 0x0300,
	0x6F12, 0x0141,
	0x6F12, 0x0182,
	0x6F12, 0x03C3,
	0x6F12, 0x0302,
	0x602A, 0x5960,
	0x6F12, 0x0500,
	0x602A, 0x5964,
	0x6F12, 0x3F01,
	0x602A, 0x5968,
	0x6F12, 0x3F01,
	0x602A, 0x596C,
	0x6F12, 0x3F05,
	0x602A, 0x5970,
	0x6F12, 0x3F00,
	0x602A, 0x5974,
	0x6F12, 0x3F04,
	0x602A, 0x5978,
	0x6F12, 0x3F01,
	0x602A, 0x597C,
	0x6F12, 0x3F05,
	0x602A, 0x59A0,
	0x6F12, 0x3F00,
	0x602A, 0x59A4,
	0x6F12, 0x3F01,
	0x602A, 0x59A8,
	0x6F12, 0x3F01,
	0x602A, 0x59AC,
	0x6F12, 0x3F05,
	0x602A, 0x59B0,
	0x6F12, 0x3F00,
	0x602A, 0x59B4,
	0x6F12, 0x3F04,
	0x602A, 0x59B8,
	0x6F12, 0x3F01,
	0x602A, 0x59BC,
	0x6F12, 0x3F05,
	0x602A, 0x59E0,
	0x6F12, 0x3F00,
	0x602A, 0x59E4,
	0x6F12, 0x3F01,
	0x602A, 0x59E8,
	0x6F12, 0x3F3F,
	0x602A, 0x59EC,
	0x6F12, 0x3F3F,
	0x602A, 0x59F0,
	0x6F12, 0x3F3F,
	0x602A, 0x59F4,
	0x6F12, 0x3F3F,
	0x602A, 0x59F8,
	0x6F12, 0x3F3F,
	0x602A, 0x59FC,
	0x6F12, 0x3F3F,
	0x602A, 0x5A20,
	0x6F12, 0x3F00,
	0x602A, 0x5A24,
	0x6F12, 0x3F01,
	0x602A, 0x5A28,
	0x6F12, 0x3F3F,
	0x602A, 0x5A2C,
	0x6F12, 0x3F3F,
	0x602A, 0x5A30,
	0x6F12, 0x3F3F,
	0x602A, 0x5A34,
	0x6F12, 0x3F3F,
	0x602A, 0x5A38,
	0x6F12, 0x3F3F,
	0x602A, 0x5A3C,
	0x6F12, 0x3F3F,
	0x602A, 0x58EC,
	0x6F12, 0x000F,
	0x602A, 0x5930,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x2001,
	0x602A, 0xF800,
	0x6F12, 0x02D8,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x00E4,
	0x6F12, 0x004E,
	0x6F12, 0x0169,
	0x602A, 0x8680,
	0x6F12, 0x0101,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x207A,
	0x6F12, 0x0A00,
	0x0D06, 0x01E0,
	0x0D08, 0x086C,
	0x0B06, 0x0101,
	0x0B08, 0x0000,
	0x0BC8, 0x0001,
	0x602A, 0x21BC,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x602A, 0x37BC,
	0x6F12, 0x0100,
	0x602A, 0x37C2,
	0x6F12, 0x0200,
	0x6028, 0x2001,
	0x602A, 0xFD6E,
	0x6F12, 0x0200,
	0x602A, 0xF88E,
	0x6F12, 0x0100,
	0x602A, 0xF8A0,
	0x6F12, 0x0050,
	0x6F12, 0x0060,
	0x6F12, 0x0070,
	0x602A, 0xF8A6,
	0x6F12, 0x0000,
	0x602A, 0xF8C2,
	0x6F12, 0x0000,
	0x602A, 0xFD70,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0xF8D8,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x642C,
	0x6F12, 0x0078,
	0x6F12, 0x0052,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x6444,
	0x6F12, 0x0001,
	0x602A, 0x815E,
	0x6F12, 0x0047,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0048,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0049,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x602A, 0x81C2,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x602A, 0x85EE,
	0x6F12, 0x00A4,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A5,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A6,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00A7,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x00A8,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A9,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AA,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00AB,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x00AC,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AD,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AE,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00AF,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x602A, 0x87BE,
	0x6F12, 0x00D0,
	0x6F12, 0x0020,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x00D1,
	0x6F12, 0x0020,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x00D2,
	0x6F12, 0x0020,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x602A, 0x3DD4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x8342,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x602A, 0x8352,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x8212,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x602A, 0x8222,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x602A, 0x8452,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x602A, 0x8462,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x602A, 0x8572,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x602A, 0x8582,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x602A, 0x5270,
	0x6F12, 0x0000,
	0x602A, 0x5C80,
	0x6F12, 0x0000,
	0x602A, 0x3D32,
	0x6F12, 0x0001,
	0x6F12, 0x0103,
	0x602A, 0x134C,
	0x6F12, 0x0340,
	0x602A, 0x21C0,
	0x6F12, 0x0000,
	0x0FE8, 0x3600,
	0x0B00, 0x0180,
	0x602A, 0x4FB0,
	0x6F12, 0x0301,
	0x602A, 0x4FD2,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x4FCE,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x591E,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x42AE,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x4966,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x36CE,
	0x6F12, 0x0001,
	0x6F12, 0x0003,
	0x6F12, 0x000E,
	0x6028, 0x2000,
	0x602A, 0x40F8,
	0x6F12, 0x0000,
	0x6028, 0x2001,
	0x602A, 0xF8DA,
	0x6F12, 0x0000,
	0x6F12, 0x04F0,
	0x6F12, 0x00F0,
	0x6F12, 0xAAE3,
	0x6F12, 0x200A,
	0x6F12, 0x0020,
	0x6F12, 0x551E,
	0x6F12, 0x200A,
	0x6F12, 0x0020,
	0x6F12, 0xAA39,
	0x6F12, 0x200A,
	0x6F12, 0x0020,
	0x6F12, 0x55F2,
	0x6F12, 0x200A,
	0x6F12, 0x0022,
	0x6F12, 0x00A8,
	0x6F12, 0x2000,
	0x6F12, 0x0000,
	0x6028, 0x4000,
	0x6214, 0xF9F0,
	0x6218, 0xF9F0,
	0x0100, 0x0100,
};
#endif
static void normal_video_setting(void)
{




	
	/*	ZTE D2A2 for PDAF tail mode 2bin D2A2 2000x1500 30fps PDAF correc always on   */
  LOG_INF("E\n");
  #if 0
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x62C4);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x44DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x0007);
write_cmos_sensor(0xF448, 0x0000);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x002D);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFC00);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0010);
write_cmos_sensor(0x6F12, 0x0015);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x002A);
write_cmos_sensor(0x6F12, 0x0035);
write_cmos_sensor(0x6F12, 0x003A);
write_cmos_sensor(0x602A, 0xFD4E);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x2000);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x2002);
write_cmos_sensor(0x6F12, 0x00BF);
write_cmos_sensor(0x602A, 0x2006);
write_cmos_sensor(0x6F12, 0x00C0);
write_cmos_sensor(0x602A, 0x200A);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x200C);
write_cmos_sensor(0x6F12, 0x4000);
write_cmos_sensor(0x602A, 0x200E);
write_cmos_sensor(0x6F12, 0xF402);
write_cmos_sensor(0x602A, 0x2010);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2012);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x602A, 0x2014);
write_cmos_sensor(0x6F12, 0xF404);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x4040);
write_cmos_sensor(0x0340, 0x0F32);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0000);
write_cmos_sensor(0x0310, 0x00C0);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0000);//czy chg
write_cmos_sensor(0x0D02, 0x0101);//Tail Mode ON 0x0101 0x0000
write_cmos_sensor(0x0114, 0x0301);//Tail Mode ON 0x0301 0x0300
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0BC8, 0x0001);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x37C2);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xFD6E);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8C2);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFD70);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x87BE);
write_cmos_sensor(0x6F12, 0x00D0);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x0032);
write_cmos_sensor(0x6F12, 0x00D1);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x0092);
write_cmos_sensor(0x6F12, 0x00D2);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x6F12, 0x0028);
write_cmos_sensor(0x602A, 0x3DD4);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x8342);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x8352);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x6F12, 0x00F0);
write_cmos_sensor(0x602A, 0x8212);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x8222);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x6F12, 0x00C3);
write_cmos_sensor(0x602A, 0x8452);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x6F12, 0x008C);
write_cmos_sensor(0x602A, 0x8462);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x6F12, 0x0104);
write_cmos_sensor(0x602A, 0x8572);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x6F12, 0x0127);
write_cmos_sensor(0x602A, 0x8582);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x6F12, 0x010A);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0340);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0180);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0301);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x591E);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x6F12, 0xFFC0);
write_cmos_sensor(0x602A, 0x42AE);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x4966);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36CE);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0003);
write_cmos_sensor(0x6F12, 0x000E);
//iqv
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);                                  
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#else
	write_cmos_sensor(0x6028, 0x4000); 
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF150);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x36AA);
write_cmos_sensor(0x6F12, 0x0020);
write_cmos_sensor(0x6F12, 0x0021);
write_cmos_sensor(0x6F12, 0x0030);
write_cmos_sensor(0x6F12, 0x0031);
write_cmos_sensor(0x602A, 0x36CA);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6F12, 0x1FFF);
write_cmos_sensor(0x6A12, 0x0001);
write_cmos_sensor(0x602A, 0x2090);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x3670);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x368C);
write_cmos_sensor(0x6F12, 0x387F);
write_cmos_sensor(0x6F12, 0xF84C);
write_cmos_sensor(0x602A, 0x37B0);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6F12, 0x0018);
write_cmos_sensor(0x6F12, 0x0014);
write_cmos_sensor(0x6F12, 0x0013);
write_cmos_sensor(0x602A, 0x37BA);
write_cmos_sensor(0x6F12, 0x0011);
write_cmos_sensor(0x602A, 0x3680);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0280);
write_cmos_sensor(0x6F12, 0x5008);
write_cmos_sensor(0x602A, 0x235C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x37A8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x36D6);
write_cmos_sensor(0x6F12, 0x04DE);
write_cmos_sensor(0x6F12, 0x40AE);
write_cmos_sensor(0x6F12, 0xC000);
write_cmos_sensor(0xF446, 0x023F);
write_cmos_sensor(0xF448, 0x00A1);
write_cmos_sensor(0x602A, 0x37A0);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x2594);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x259C);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25A4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25AC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25B4);
write_cmos_sensor(0x6F12, 0x002B);
write_cmos_sensor(0x602A, 0x25BC);
write_cmos_sensor(0x6F12, 0x05EA);
write_cmos_sensor(0x602A, 0x25C4);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x25CC);
write_cmos_sensor(0x6F12, 0x05FF);
write_cmos_sensor(0x602A, 0x3690);
write_cmos_sensor(0x6F12, 0xFFEA);
write_cmos_sensor(0x602A, 0x285C);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x286C);
write_cmos_sensor(0x6F12, 0x0234);
write_cmos_sensor(0x602A, 0x2474);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x602A, 0x247C);
write_cmos_sensor(0x6F12, 0x00F8);
write_cmos_sensor(0x602A, 0x3694);
write_cmos_sensor(0x6F12, 0x000F);
write_cmos_sensor(0x602A, 0x2C62);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8001);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x6F12, 0x8030);
write_cmos_sensor(0x602A, 0x236C);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x602A, 0x249C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x602A, 0x3686);
write_cmos_sensor(0x6F12, 0x0808);
write_cmos_sensor(0x602A, 0x37A2);
write_cmos_sensor(0x6F12, 0x0025);
write_cmos_sensor(0x602A, 0x1388);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2124);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x1E00);
write_cmos_sensor(0x602A, 0x3EA4);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x3E00);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x3678);
write_cmos_sensor(0x6F12, 0x0702);
write_cmos_sensor(0x602A, 0x2344);
write_cmos_sensor(0x6F12, 0x0027);
write_cmos_sensor(0x602A, 0x2354);
write_cmos_sensor(0x6F12, 0x0066);
write_cmos_sensor(0x602A, 0x2384);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x2464);
write_cmos_sensor(0x6F12, 0x000C);
write_cmos_sensor(0x602A, 0x248C);
write_cmos_sensor(0x6F12, 0x000A);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0x4F4C);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xFE44);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x00A3);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x0110, 0x1002);
write_cmos_sensor(0x011C, 0x0100);
write_cmos_sensor(0x0344, 0x0000);
write_cmos_sensor(0x0346, 0x0000);
write_cmos_sensor(0x0348, 0x243F);
write_cmos_sensor(0x034A, 0x1B1F);
write_cmos_sensor(0x034C, 0x1220);
write_cmos_sensor(0x034E, 0x0D90);
write_cmos_sensor(0x0350, 0x0000);
write_cmos_sensor(0x0352, 0x0000);
write_cmos_sensor(0x0342, 0x83B0);
write_cmos_sensor(0x0340, 0x0DFE);
write_cmos_sensor(0x602A, 0x3652);
write_cmos_sensor(0x6F12, 0x0401);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0002);
write_cmos_sensor(0x0386, 0x0002);
write_cmos_sensor(0x0900, 0x0212);
write_cmos_sensor(0x040C, 0x1000);
write_cmos_sensor(0x0404, 0x1000);
write_cmos_sensor(0x0408, 0x0100);
write_cmos_sensor(0x040A, 0x0100);
write_cmos_sensor(0x0400, 0x2010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0304, 0x0004);
write_cmos_sensor(0x030C, 0x0000);
write_cmos_sensor(0x0306, 0x00A0);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0300, 0x0004);
write_cmos_sensor(0x030E, 0x0004);
write_cmos_sensor(0x0312, 0x0002);
write_cmos_sensor(0x0310, 0x012C);
write_cmos_sensor(0x602A, 0x368A);
write_cmos_sensor(0x6F12, 0x0096);
write_cmos_sensor(0x021E, 0x0000);
write_cmos_sensor(0x0D00, 0x0100);
write_cmos_sensor(0x0D02, 0x0000);
write_cmos_sensor(0x0114, 0x0300);
write_cmos_sensor(0x602A, 0x593C);
write_cmos_sensor(0x6F12, 0x0110);
write_cmos_sensor(0x6F12, 0x0402);
write_cmos_sensor(0x6F12, 0x040C);
write_cmos_sensor(0x6F12, 0x024E);
write_cmos_sensor(0x6F12, 0x0284);
write_cmos_sensor(0x6F12, 0x01C6);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6F12, 0x0242);
write_cmos_sensor(0x6F12, 0x0288);
write_cmos_sensor(0x6F12, 0x01CA);
write_cmos_sensor(0x6F12, 0x0102);
write_cmos_sensor(0x602A, 0x5960);
write_cmos_sensor(0x6F12, 0x0500);
write_cmos_sensor(0x602A, 0x5964);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5968);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x596C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5970);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5974);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5978);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x597C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59A4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59A8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59AC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59B4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59B8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59BC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59E4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59E8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59EC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F0);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59F4);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x59F8);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x59FC);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A20);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A24);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A28);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A2C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A30);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A34);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x5A38);
write_cmos_sensor(0x6F12, 0x3F00);
write_cmos_sensor(0x602A, 0x5A3C);
write_cmos_sensor(0x6F12, 0x3F01);
write_cmos_sensor(0x602A, 0x58EC);
write_cmos_sensor(0x6F12, 0x0103);
write_cmos_sensor(0x602A, 0x5930);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF800);
write_cmos_sensor(0x6F12, 0x0264);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x5050);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x004E);
write_cmos_sensor(0x6F12, 0x00E4);
write_cmos_sensor(0x6F12, 0x004A);
write_cmos_sensor(0x602A, 0x8680);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x207A);
write_cmos_sensor(0x6F12, 0x0A00);
write_cmos_sensor(0x0D06, 0x0248);
write_cmos_sensor(0x0D08, 0x0D8C);
write_cmos_sensor(0x0B06, 0x0101);
write_cmos_sensor(0x0B08, 0x0000);
write_cmos_sensor(0x0FEA, 0x2440);
write_cmos_sensor(0x602A, 0x21BC);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x0202, 0x0010);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x602A, 0x37BC);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF88E);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8A0);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x0060);
write_cmos_sensor(0x6F12, 0x0070);
write_cmos_sensor(0x602A, 0xF8A6);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8D8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0xF8DA);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x40F8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6028, 0x2001);
write_cmos_sensor(0x602A, 0xF8DC);
write_cmos_sensor(0x6F12, 0x05E8);
write_cmos_sensor(0x6F12, 0x0050);
write_cmos_sensor(0x6F12, 0x88C4);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x00EB);
write_cmos_sensor(0x6F12, 0x2000);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x111E);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x22FD);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6F12, 0x44D8);
write_cmos_sensor(0x6F12, 0x2011);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x642C);
write_cmos_sensor(0x6F12, 0x0078);
write_cmos_sensor(0x6F12, 0x0052);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x6444);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x815E);
write_cmos_sensor(0x6F12, 0x0047);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0048);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0049);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x6F12, 0x0019);
write_cmos_sensor(0x602A, 0x81C2);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x6F12, 0x003C);
write_cmos_sensor(0x602A, 0x85EE);
write_cmos_sensor(0x6F12, 0x00A4);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A5);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A6);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00A7);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00A8);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00A9);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AA);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AB);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x00AC);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AD);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x00AE);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x0005);
write_cmos_sensor(0x6F12, 0x00AF);
write_cmos_sensor(0x6F12, 0x0004);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x6F12, 0x0006);
write_cmos_sensor(0x602A, 0x5270);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x5C80);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x3D32);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x6F12, 0x0203);
write_cmos_sensor(0x602A, 0x134C);
write_cmos_sensor(0x6F12, 0x0300);
write_cmos_sensor(0x602A, 0x2336);
write_cmos_sensor(0x6F12, 0xFFFE);
write_cmos_sensor(0x602A, 0x21C0);
write_cmos_sensor(0x6F12, 0x1100);
write_cmos_sensor(0x0FE8, 0x3600);
write_cmos_sensor(0x0B00, 0x0080);
write_cmos_sensor(0x602A, 0x4FB0);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x602A, 0x4FD2);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x602A, 0x4FCE);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x6F12, 0x0202);
write_cmos_sensor(0x081E, 0x0A00);
write_cmos_sensor(0x6B5C, 0xE222);
write_cmos_sensor(0x6B5E, 0x00FF);
write_cmos_sensor(0x6B76, 0xE000);
write_cmos_sensor(0x6028, 0x4000);
write_cmos_sensor(0x6214, 0xF9F0);
write_cmos_sensor(0x6218, 0xF9F0);
#endif

    #if 0
	LOG_INF("%s E! currefps 30\n", __func__);
	s5kgw1_table_write_cmos_sensor(s5kgw1_normal_video_setting,
		sizeof(s5kgw1_normal_video_setting)/sizeof(kal_uint16));
	LOG_INF("X\n");
	#endif
}

static kal_uint16 s5kgw1_hs_video_setting[] = {
	0x6028, 0x4000,
	0x6214, 0xF9F0,
	0x6218, 0xF150,
	0x6028, 0x2000,
	0x602A, 0x36AA,
	0x6F12, 0x0060,
	0x6F12, 0x0061,
	0x6F12, 0x0070,
	0x6F12, 0x0071,
	0x602A, 0x36CA,
	0x6F12, 0x1FFF,
	0x6F12, 0x1FFF,
	0x6A12, 0x0000,
	0x602A, 0x2090,
	0x6F12, 0xFFFF,
	0x602A, 0x62C4,
	0x6F12, 0x0100,
	0x6F12, 0x0000,
	0x602A, 0x3670,
	0x6F12, 0x0505,
	0x6F12, 0x0505,
	0x602A, 0x368C,
	0x6F12, 0x387F,
	0x6F12, 0xFFFC,
	0x602A, 0x37B0,
	0x6F12, 0x000A,
	0x6F12, 0x0018,
	0x6F12, 0x0014,
	0x6F12, 0x0013,
	0x602A, 0x37BA,
	0x6F12, 0x0011,
	0x602A, 0x3680,
	0x6F12, 0x0000,
	0x6F12, 0x0280,
	0x6F12, 0x5008,
	0x602A, 0x235C,
	0x6F12, 0x0009,
	0x602A, 0x37A8,
	0x6F12, 0x0000,
	0x602A, 0x36D6,
	0x6F12, 0x44DE,
	0x6F12, 0x80AE,
	0x6F12, 0xC000,
	0xF446, 0x0007,
	0xF448, 0x0000,
	0x602A, 0x37A0,
	0x6F12, 0x0000,
	0x602A, 0x2594,
	0x6F12, 0x002B,
	0x602A, 0x259C,
	0x6F12, 0x05EA,
	0x602A, 0x25A4,
	0x6F12, 0x05FF,
	0x602A, 0x25AC,
	0x6F12, 0x05FF,
	0x602A, 0x25B4,
	0x6F12, 0x002B,
	0x602A, 0x25BC,
	0x6F12, 0x05EA,
	0x602A, 0x25C4,
	0x6F12, 0x05FF,
	0x602A, 0x25CC,
	0x6F12, 0x05FF,
	0x602A, 0x3690,
	0x6F12, 0xFFEA,
	0x602A, 0x285C,
	0x6F12, 0x0101,
	0x602A, 0x286C,
	0x6F12, 0x0234,
	0x602A, 0x2474,
	0x6F12, 0x0110,
	0x602A, 0x247C,
	0x6F12, 0x00F8,
	0x602A, 0x3694,
	0x6F12, 0x000F,
	0x602A, 0x2C62,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8001,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x6F12, 0x8030,
	0x602A, 0x236C,
	0x6F12, 0x0009,
	0x602A, 0x249C,
	0x6F12, 0x000A,
	0x602A, 0x3686,
	0x6F12, 0x0808,
	0x602A, 0x37A2,
	0x6F12, 0x002D,
	0x602A, 0x1388,
	0x6F12, 0x0080,
	0x602A, 0x2124,
	0x6F12, 0x0100,
	0x6F12, 0x1E00,
	0x602A, 0x3EA4,
	0x6F12, 0x0003,
	0x602A, 0x3E00,
	0x6F12, 0x0201,
	0x602A, 0x3678,
	0x6F12, 0x0707,
	0x602A, 0x2344,
	0x6F12, 0x0023,
	0x602A, 0x2354,
	0x6F12, 0x008C,
	0x602A, 0x2384,
	0x6F12, 0x0090,
	0x602A, 0x2464,
	0x6F12, 0x0012,
	0x602A, 0x248C,
	0x6F12, 0x0011,
	0x6028, 0x2001,
	0x602A, 0x4F4C,
	0x6F12, 0x0000,
	0x602A, 0xFC00,
	0x6F12, 0x0101,
	0x6F12, 0x0101,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x00A3,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0011,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0033,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0022,
	0x6F12, 0x0000,
	0x6F12, 0x0032,
	0x6F12, 0x0000,
	0x602A, 0xFD4E,
	0x6F12, 0x4000,
	0x6F12, 0x0003,
	0x6028, 0x2000,
	0x602A, 0x2000,
	0x6F12, 0x0100,
	0x602A, 0x2002,
	0x6F12, 0x00BF,
	0x602A, 0x2006,
	0x6F12, 0x00C0,
	0x602A, 0x200A,
	0x6F12, 0x0000,
	0x602A, 0x200C,
	0x6F12, 0x4000,
	0x602A, 0x200E,
	0x6F12, 0xF402,
	0x602A, 0x2010,
	0x6F12, 0x0000,
	0x602A, 0x2012,
	0x6F12, 0x0003,
	0x602A, 0x2014,
	0x6F12, 0xF404,
	0x0110, 0x1002,
	0x011C, 0x0100,
	0x0344, 0x0320,
	0x0346, 0x0520,
	0x0348, 0x211F,
	0x034A, 0x15FF,
	0x034C, 0x0780,
	0x034E, 0x0438,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0342, 0x3640,
	0x0340, 0x0480,
	0x602A, 0x3652,
	0x6F12, 0x0401,
	0x0380, 0x0002,
	0x0382, 0x0006,
	0x0384, 0x0002,
	0x0386, 0x0006,
	0x0900, 0x3344,
	0x040C, 0x0000,
	0x0404, 0x1000,
	0x0408, 0x0100,
	0x040A, 0x0100,
	0x0400, 0x1010,
	0x0136, 0x1800,
	0x0304, 0x0004,
	0x030C, 0x0000,
	0x0306, 0x00A0,
	0x0302, 0x0001,
	0x0300, 0x0004,
	0x030E, 0x0004,
	0x0312, 0x0002,
	0x0310, 0x0131,
	0x602A, 0x368A,
	0x6F12, 0x0096,
	0x021E, 0x0000,
	0x0D00, 0x0000,
	0x0D02, 0x0000,
	0x0114, 0x0300,
	0x602A, 0x593C,
	0x6F12, 0x0104,
	0x6F12, 0x0802,
	0x6F12, 0x0100,
	0x6F12, 0x0141,
	0x6F12, 0x0182,
	0x6F12, 0x03C3,
	0x6F12, 0x0300,
	0x6F12, 0x0141,
	0x6F12, 0x0182,
	0x6F12, 0x03C3,
	0x6F12, 0x0302,
	0x602A, 0x5960,
	0x6F12, 0x0500,
	0x602A, 0x5964,
	0x6F12, 0x3F01,
	0x602A, 0x5968,
	0x6F12, 0x3F01,
	0x602A, 0x596C,
	0x6F12, 0x3F05,
	0x602A, 0x5970,
	0x6F12, 0x3F00,
	0x602A, 0x5974,
	0x6F12, 0x3F04,
	0x602A, 0x5978,
	0x6F12, 0x3F01,
	0x602A, 0x597C,
	0x6F12, 0x3F05,
	0x602A, 0x59A0,
	0x6F12, 0x3F00,
	0x602A, 0x59A4,
	0x6F12, 0x3F01,
	0x602A, 0x59A8,
	0x6F12, 0x3F01,
	0x602A, 0x59AC,
	0x6F12, 0x3F05,
	0x602A, 0x59B0,
	0x6F12, 0x3F00,
	0x602A, 0x59B4,
	0x6F12, 0x3F04,
	0x602A, 0x59B8,
	0x6F12, 0x3F01,
	0x602A, 0x59BC,
	0x6F12, 0x3F05,
	0x602A, 0x59E0,
	0x6F12, 0x3F00,
	0x602A, 0x59E4,
	0x6F12, 0x3F01,
	0x602A, 0x59E8,
	0x6F12, 0x3F3F,
	0x602A, 0x59EC,
	0x6F12, 0x3F3F,
	0x602A, 0x59F0,
	0x6F12, 0x3F3F,
	0x602A, 0x59F4,
	0x6F12, 0x3F3F,
	0x602A, 0x59F8,
	0x6F12, 0x3F3F,
	0x602A, 0x59FC,
	0x6F12, 0x3F3F,
	0x602A, 0x5A20,
	0x6F12, 0x3F00,
	0x602A, 0x5A24,
	0x6F12, 0x3F01,
	0x602A, 0x5A28,
	0x6F12, 0x3F3F,
	0x602A, 0x5A2C,
	0x6F12, 0x3F3F,
	0x602A, 0x5A30,
	0x6F12, 0x3F3F,
	0x602A, 0x5A34,
	0x6F12, 0x3F3F,
	0x602A, 0x5A38,
	0x6F12, 0x3F3F,
	0x602A, 0x5A3C,
	0x6F12, 0x3F3F,
	0x602A, 0x58EC,
	0x6F12, 0x000F,
	0x602A, 0x5930,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x2001,
	0x602A, 0xF800,
	0x6F12, 0x02D8,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x00E4,
	0x6F12, 0x004E,
	0x6F12, 0x0169,
	0x602A, 0x8680,
	0x6F12, 0x0101,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x207A,
	0x6F12, 0x0A00,
	0x0D06, 0x01E0,
	0x0D08, 0x086C,
	0x0B06, 0x0101,
	0x0B08, 0x0000,
	0x0BC8, 0x0001,
	0x602A, 0x21BC,
	0x6F12, 0x0000,
	0x6F12, 0x0002,
	0x602A, 0x37BC,
	0x6F12, 0x0100,
	0x602A, 0x37C2,
	0x6F12, 0x0200,
	0x6028, 0x2001,
	0x602A, 0xFD6E,
	0x6F12, 0x0200,
	0x602A, 0xF88E,
	0x6F12, 0x0100,
	0x602A, 0xF8A0,
	0x6F12, 0x0050,
	0x6F12, 0x0060,
	0x6F12, 0x0070,
	0x602A, 0xF8A6,
	0x6F12, 0x0000,
	0x602A, 0xF8C2,
	0x6F12, 0x0000,
	0x602A, 0xFD70,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0xF8D8,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x642C,
	0x6F12, 0x0078,
	0x6F12, 0x0052,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x6444,
	0x6F12, 0x0001,
	0x602A, 0x815E,
	0x6F12, 0x0047,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0048,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0049,
	0x6F12, 0x0004,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x6F12, 0x0019,
	0x602A, 0x81C2,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x6F12, 0x003C,
	0x602A, 0x85EE,
	0x6F12, 0x00A4,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A5,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A6,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00A7,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x00A8,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00A9,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AA,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00AB,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x00AC,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AD,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x00AE,
	0x6F12, 0x0004,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x0005,
	0x6F12, 0x00AF,
	0x6F12, 0x0004,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x6F12, 0x0006,
	0x602A, 0x87BE,
	0x6F12, 0x00D0,
	0x6F12, 0x0020,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x0032,
	0x6F12, 0x00D1,
	0x6F12, 0x0020,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x0092,
	0x6F12, 0x00D2,
	0x6F12, 0x0020,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x6F12, 0x0028,
	0x602A, 0x3DD4,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x8342,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x6F12, 0x00A0,
	0x602A, 0x8352,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x8212,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x6F12, 0x0080,
	0x602A, 0x8222,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x6F12, 0x00C3,
	0x602A, 0x8452,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x6F12, 0x008C,
	0x602A, 0x8462,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x6F12, 0x0104,
	0x602A, 0x8572,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x6F12, 0x0127,
	0x602A, 0x8582,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x6F12, 0x010A,
	0x602A, 0x5270,
	0x6F12, 0x0000,
	0x602A, 0x5C80,
	0x6F12, 0x0000,
	0x602A, 0x3D32,
	0x6F12, 0x0001,
	0x6F12, 0x0103,
	0x602A, 0x134C,
	0x6F12, 0x0340,
	0x602A, 0x21C0,
	0x6F12, 0x0000,
	0x0FE8, 0x3600,
	0x0B00, 0x0180,
	0x602A, 0x4FB0,
	0x6F12, 0x0301,
	0x602A, 0x4FD2,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x4FCE,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x591E,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x6F12, 0xFFC0,
	0x602A, 0x42AE,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x4966,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x36CE,
	0x6F12, 0x0001,
	0x6F12, 0x0003,
	0x6F12, 0x000E,
};

static void hs_video_setting(void)
{
	LOG_INF("%s E! currefps 120\n", __func__);
	s5kgw1_table_write_cmos_sensor(s5kgw1_hs_video_setting,
		sizeof(s5kgw1_hs_video_setting)/sizeof(kal_uint16));
	LOG_INF("X\n");
}
static void slim_video_setting(void)
{
	/*	oppo 1280x720 Mass Setup, 3bin HD 1280x720 120fps, PD does not exist, tail off   */
  LOG_INF("E\n");
  write_cmos_sensor(0x6028,0x4000);
  write_cmos_sensor(0x6214,0x7971);
  write_cmos_sensor(0x6218,0x7150);
  write_cmos_sensor(0x0344,0x0058);
  write_cmos_sensor(0x0346,0x01AC);
  write_cmos_sensor(0x0348,0x0F57);
  write_cmos_sensor(0x034A,0x0A1B);
  write_cmos_sensor(0x034C,0x0500);
  write_cmos_sensor(0x034E,0x02D0);
  write_cmos_sensor(0x0350,0x0000);
  write_cmos_sensor(0x0352,0x0000);
  write_cmos_sensor(0x0340,0x0330);
  write_cmos_sensor(0x0342,0x13A0);
  write_cmos_sensor(0x0900,0x0123);
  write_cmos_sensor(0x0380,0x0001);
  write_cmos_sensor(0x0382,0x0002);
  write_cmos_sensor(0x0384,0x0001);
  write_cmos_sensor(0x0386,0x0005);
  write_cmos_sensor(0x0404,0x1000);
  write_cmos_sensor(0x0402,0x1810);
  write_cmos_sensor(0x0136,0x1800);
  write_cmos_sensor(0x0304,0x0006);
  write_cmos_sensor(0x030C,0x0000);
  write_cmos_sensor(0x0306,0x00F6);
  write_cmos_sensor(0x0302,0x0001);
  write_cmos_sensor(0x0300,0x0008);
  write_cmos_sensor(0x030E,0x0003);
  write_cmos_sensor(0x0312,0x0002);
  write_cmos_sensor(0x0310,0x005B);
  write_cmos_sensor(0x6028,0x2000);
  write_cmos_sensor(0x602A,0x1492);
  write_cmos_sensor(0x6F12,0x0078);
  write_cmos_sensor(0x602A,0x0E4E);
  write_cmos_sensor(0x6F12,0xFFFF);
  write_cmos_sensor(0x6028,0x4000);
  write_cmos_sensor(0x0118,0x0104);
  write_cmos_sensor(0x021E,0x0000);
  write_cmos_sensor(0x6028,0x2000);
  write_cmos_sensor(0x602A,0x2126);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1168);
  write_cmos_sensor(0x6F12,0x0020);
  write_cmos_sensor(0x602A,0x2DB6);
  write_cmos_sensor(0x6F12,0x0001);
  write_cmos_sensor(0x602A,0x1668);
  write_cmos_sensor(0x6F12,0xF0F0);
  write_cmos_sensor(0x602A,0x166A);
  write_cmos_sensor(0x6F12,0xF0F0);
  write_cmos_sensor(0x602A,0x118A);
  write_cmos_sensor(0x6F12,0x0802);
  write_cmos_sensor(0x602A,0x151E);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x217E);
  write_cmos_sensor(0x6F12,0x0001);
  write_cmos_sensor(0x602A,0x1520);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x2522);
  write_cmos_sensor(0x6F12,0x0804);
  write_cmos_sensor(0x602A,0x2524);
  write_cmos_sensor(0x6F12,0x0400);
  write_cmos_sensor(0x602A,0x2568);
  write_cmos_sensor(0x6F12,0x5500);
  write_cmos_sensor(0x602A,0x2588);
  write_cmos_sensor(0x6F12,0x1111);
  write_cmos_sensor(0x602A,0x258C);
  write_cmos_sensor(0x6F12,0x1111);
  write_cmos_sensor(0x602A,0x25A6);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x252C);
  write_cmos_sensor(0x6F12,0x0601);
  write_cmos_sensor(0x602A,0x252E);
  write_cmos_sensor(0x6F12,0x0605);
  write_cmos_sensor(0x602A,0x25A8);
  write_cmos_sensor(0x6F12,0x1100);
  write_cmos_sensor(0x602A,0x25AC);
  write_cmos_sensor(0x6F12,0x0011);
  write_cmos_sensor(0x602A,0x25B0);
  write_cmos_sensor(0x6F12,0x1100);
  write_cmos_sensor(0x602A,0x25B4);
  write_cmos_sensor(0x6F12,0x0011);
  write_cmos_sensor(0x602A,0x15A4);
  write_cmos_sensor(0x6F12,0x0141);
  write_cmos_sensor(0x602A,0x15A6);
  write_cmos_sensor(0x6F12,0x0545);
  write_cmos_sensor(0x602A,0x15A8);
  write_cmos_sensor(0x6F12,0x0649);
  write_cmos_sensor(0x602A,0x15AA);
  write_cmos_sensor(0x6F12,0x024D);
  write_cmos_sensor(0x602A,0x15AC);
  write_cmos_sensor(0x6F12,0x0151);
  write_cmos_sensor(0x602A,0x15AE);
  write_cmos_sensor(0x6F12,0x0555);
  write_cmos_sensor(0x602A,0x15B0);
  write_cmos_sensor(0x6F12,0x0659);
  write_cmos_sensor(0x602A,0x15B2);
  write_cmos_sensor(0x6F12,0x025D);
  write_cmos_sensor(0x602A,0x15B4);
  write_cmos_sensor(0x6F12,0x0161);
  write_cmos_sensor(0x602A,0x15B6);
  write_cmos_sensor(0x6F12,0x0565);
  write_cmos_sensor(0x602A,0x15B8);
  write_cmos_sensor(0x6F12,0x0669);
  write_cmos_sensor(0x602A,0x15BA);
  write_cmos_sensor(0x6F12,0x026D);
  write_cmos_sensor(0x602A,0x15BC);
  write_cmos_sensor(0x6F12,0x0171);
  write_cmos_sensor(0x602A,0x15BE);
  write_cmos_sensor(0x6F12,0x0575);
  write_cmos_sensor(0x602A,0x15C0);
  write_cmos_sensor(0x6F12,0x0679);
  write_cmos_sensor(0x602A,0x15C2);
  write_cmos_sensor(0x6F12,0x027D);
  write_cmos_sensor(0x602A,0x15C4);
  write_cmos_sensor(0x6F12,0x0141);
  write_cmos_sensor(0x602A,0x15C6);
  write_cmos_sensor(0x6F12,0x0545);
  write_cmos_sensor(0x602A,0x15C8);
  write_cmos_sensor(0x6F12,0x0649);
  write_cmos_sensor(0x602A,0x15CA);
  write_cmos_sensor(0x6F12,0x024D);
  write_cmos_sensor(0x602A,0x15CC);
  write_cmos_sensor(0x6F12,0x0151);
  write_cmos_sensor(0x602A,0x15CE);
  write_cmos_sensor(0x6F12,0x0555);
  write_cmos_sensor(0x602A,0x15D0);
  write_cmos_sensor(0x6F12,0x0659);
  write_cmos_sensor(0x602A,0x15D2);
  write_cmos_sensor(0x6F12,0x025D);
  write_cmos_sensor(0x602A,0x15D4);
  write_cmos_sensor(0x6F12,0x0161);
  write_cmos_sensor(0x602A,0x15D6);
  write_cmos_sensor(0x6F12,0x0565);
  write_cmos_sensor(0x602A,0x15D8);
  write_cmos_sensor(0x6F12,0x0669);
  write_cmos_sensor(0x602A,0x15DA);
  write_cmos_sensor(0x6F12,0x026D);
  write_cmos_sensor(0x602A,0x15DC);
  write_cmos_sensor(0x6F12,0x0171);
  write_cmos_sensor(0x602A,0x15DE);
  write_cmos_sensor(0x6F12,0x0575);
  write_cmos_sensor(0x602A,0x15E0);
  write_cmos_sensor(0x6F12,0x0679);
  write_cmos_sensor(0x602A,0x15E2);
  write_cmos_sensor(0x6F12,0x027D);
  write_cmos_sensor(0x602A,0x1A50);
  write_cmos_sensor(0x6F12,0x0001);
  write_cmos_sensor(0x602A,0x1A54);
  write_cmos_sensor(0x6F12,0x0100);
  write_cmos_sensor(0x6028,0x4000);
  write_cmos_sensor(0x0D00,0x0100);
  write_cmos_sensor(0x0D02,0x0001);
  write_cmos_sensor(0x0114,0x0300);
  write_cmos_sensor(0xF486,0x0000);
  write_cmos_sensor(0xF488,0x0000);
  write_cmos_sensor(0xF48A,0x0000);
  write_cmos_sensor(0xF48C,0x0000);
  write_cmos_sensor(0xF48E,0x0000);
  write_cmos_sensor(0xF490,0x0000);
  write_cmos_sensor(0xF492,0x0000);
  write_cmos_sensor(0xF494,0x0000);
  write_cmos_sensor(0xF496,0x0000);
  write_cmos_sensor(0xF498,0x0000);
  write_cmos_sensor(0xF49A,0x0000);
  write_cmos_sensor(0xF49C,0x0000);
  write_cmos_sensor(0xF49E,0x0000);
  write_cmos_sensor(0xF4A0,0x0000);
  write_cmos_sensor(0xF4A2,0x0000);
  write_cmos_sensor(0xF4A4,0x0000);
  write_cmos_sensor(0xF4A6,0x0000);
  write_cmos_sensor(0xF4A8,0x0000);
  write_cmos_sensor(0xF4AA,0x0000);
  write_cmos_sensor(0xF4AC,0x0000);
  write_cmos_sensor(0xF4AE,0x0000);
  write_cmos_sensor(0xF4B0,0x0000);
  write_cmos_sensor(0xF4B2,0x0000);
  write_cmos_sensor(0xF4B4,0x0000);
  write_cmos_sensor(0xF4B6,0x0000);
  write_cmos_sensor(0xF4B8,0x0000);
  write_cmos_sensor(0xF4BA,0x0000);
  write_cmos_sensor(0xF4BC,0x0000);
  write_cmos_sensor(0xF4BE,0x0000);
  write_cmos_sensor(0xF4C0,0x0000);
  write_cmos_sensor(0xF4C2,0x0000);
  write_cmos_sensor(0xF4C4,0x0000);
  write_cmos_sensor(0x0202,0x0010);
  write_cmos_sensor(0x0226,0x0010);
  write_cmos_sensor(0x0204,0x0020);
  write_cmos_sensor(0x0B06,0x0101);
  write_cmos_sensor(0x6028,0x2000);
  write_cmos_sensor(0x602A,0x107A);
  write_cmos_sensor(0x6F12,0x1D00);
  write_cmos_sensor(0x602A,0x1074);
  write_cmos_sensor(0x6F12,0x1D00);
  write_cmos_sensor(0x602A,0x0E7C);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1120);
  write_cmos_sensor(0x6F12,0x0200);
  write_cmos_sensor(0x602A,0x1122);
  write_cmos_sensor(0x6F12,0x0028);
  write_cmos_sensor(0x602A,0x1128);
  write_cmos_sensor(0x6F12,0x0604);
  write_cmos_sensor(0x602A,0x1AC0);
  write_cmos_sensor(0x6F12,0x0200);
  write_cmos_sensor(0x602A,0x1AC2);
  write_cmos_sensor(0x6F12,0x0002);
  write_cmos_sensor(0x602A,0x1494);
  write_cmos_sensor(0x6F12,0x3D68);
  write_cmos_sensor(0x602A,0x1498);
  write_cmos_sensor(0x6F12,0xF10D);
  write_cmos_sensor(0x602A,0x1488);
  write_cmos_sensor(0x6F12,0x0F0F);
  write_cmos_sensor(0x602A,0x148A);
  write_cmos_sensor(0x6F12,0x170F);
  write_cmos_sensor(0x602A,0x150E);
  write_cmos_sensor(0x6F12,0x00C2);
  write_cmos_sensor(0x602A,0x1510);
  write_cmos_sensor(0x6F12,0xC0AF);
  write_cmos_sensor(0x602A,0x1512);
  write_cmos_sensor(0x6F12,0x0080);
  write_cmos_sensor(0x602A,0x1486);
  write_cmos_sensor(0x6F12,0x1430);
  write_cmos_sensor(0x602A,0x1490);
  write_cmos_sensor(0x6F12,0x4D09);
  write_cmos_sensor(0x602A,0x149E);
  write_cmos_sensor(0x6F12,0x01C4);
  write_cmos_sensor(0x602A,0x11CC);
  write_cmos_sensor(0x6F12,0x0008);
  write_cmos_sensor(0x602A,0x11CE);
  write_cmos_sensor(0x6F12,0x000B);
  write_cmos_sensor(0x602A,0x11D0);
  write_cmos_sensor(0x6F12,0x0003);
  write_cmos_sensor(0x602A,0x11DA);
  write_cmos_sensor(0x6F12,0x0012);
  write_cmos_sensor(0x602A,0x11E6);
  write_cmos_sensor(0x6F12,0x002A);
  write_cmos_sensor(0x602A,0x125E);
  write_cmos_sensor(0x6F12,0x0048);
  write_cmos_sensor(0x602A,0x11F4);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x11F8);
  write_cmos_sensor(0x6F12,0x0016);
  write_cmos_sensor(0x6028,0x4000);
  write_cmos_sensor(0xF444,0x05BF);
  write_cmos_sensor(0xF44A,0x0008);
  write_cmos_sensor(0xF44E,0x0012);
  write_cmos_sensor(0xF46E,0x6CC0);
  write_cmos_sensor(0xF470,0x7809);
  write_cmos_sensor(0x6028,0x2000);
  write_cmos_sensor(0x602A,0x1CAA);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CAC);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CAE);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CB0);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CB2);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CB4);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CB6);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CB8);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CBA);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CBC);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CBE);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CC0);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CC2);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CC4);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CC6);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1CC8);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x6000);
  write_cmos_sensor(0x6F12,0x000F);
  write_cmos_sensor(0x602A,0x6002);
  write_cmos_sensor(0x6F12,0xFFFF);
  write_cmos_sensor(0x602A,0x6004);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x6006);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6008);	
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x600A);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x600C);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x600E);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6010);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6012);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6014);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6016);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6018);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x601A);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x601C);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x601E);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6020);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6022);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6024);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6026);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x6028);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x602A);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x602C);
  write_cmos_sensor(0x6F12,0x1000);
  write_cmos_sensor(0x602A,0x1144);
  write_cmos_sensor(0x6F12,0x0100);
  write_cmos_sensor(0x602A,0x1146);
  write_cmos_sensor(0x6F12,0x1B00);
  write_cmos_sensor(0x602A,0x1080);
  write_cmos_sensor(0x6F12,0x0100);
  write_cmos_sensor(0x602A,0x1084);
  write_cmos_sensor(0x6F12,0x00C0);	
  write_cmos_sensor(0x602A,0x108A);
  write_cmos_sensor(0x6F12,0x00C0);
  write_cmos_sensor(0x602A,0x1090);
  write_cmos_sensor(0x6F12,0x0001);
  write_cmos_sensor(0x602A,0x1092);
  write_cmos_sensor(0x6F12,0x0000);
  write_cmos_sensor(0x602A,0x1094);
  write_cmos_sensor(0x6F12,0xA32E);
}
static kal_uint32 return_sensor_id(void)
{
	printk("yz debug s5kgw1sp id 0x%x  (03-02-14)\n",((read_cmos_sensor_byte(0x0000) << 8) | read_cmos_sensor_byte(0x0001))); //agold add 第一次读取ID概率fai
    return ((read_cmos_sensor_byte(0x0000) << 8) | read_cmos_sensor_byte(0x0001));
}
/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID 
*
* PARAMETERS
*	*sensorID : return the sensor ID 
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
extern int hbb_flag;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 5;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    /*if(hbb_flag == 0)
    {
        while(1){
            while (imgsensor_info.i2c_addr_table[i] != 0xff) {
                spin_lock(&imgsensor_drv_lock);
                imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
                spin_unlock(&imgsensor_drv_lock);
                do {
                    *sensor_id = return_sensor_id();
                    if (*sensor_id == imgsensor_info.sensor_id) {
        #ifdef CONFIG_MTK_CAM_CAL
	            //read_imx135_otp_mtk_fmt();
        #endif
			            LOG_INF("i2c write id: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n", imgsensor.i2c_write_id,*sensor_id,imgsensor_info.sensor_id);	
                        //return ERROR_NONE;
                    }
		            LOG_INF("Read sensor id fail, i2c write id: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n", imgsensor.i2c_write_id,*sensor_id,imgsensor_info.sensor_id);	
                    retry--;
                } while(retry > 0);
                i++;
                retry = 1;
            }
        }    
    }else*/
        while (imgsensor_info.i2c_addr_table[i] != 0xff) {
            spin_lock(&imgsensor_drv_lock);
            imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
            
            spin_unlock(&imgsensor_drv_lock);
            do {
                *sensor_id = return_sensor_id();
                if (*sensor_id == imgsensor_info.sensor_id) {
    #ifdef CONFIG_MTK_CAM_CAL
		    //read_imx135_otp_mtk_fmt();
    #endif
                	//s5kgw1spCheckLensVersion(imgsensor.i2c_write_id);
				    LOG_INF("i2c write id: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n", imgsensor.i2c_write_id,*sensor_id,imgsensor_info.sensor_id);	
                    return ERROR_NONE;
                }
			    LOG_INF("Read sensor id fail, i2c write id: 0x%x, ReadOut sensor id: 0x%x, imgsensor_info.sensor_id:0x%x.\n", imgsensor.i2c_write_id,*sensor_id,imgsensor_info.sensor_id);	
                retry--;
            } while(retry > 0);
            i++;
            retry = 5;
        }    
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}
/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    //const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
    kal_uint8 i = 0;
    kal_uint8 retry = 5;
    kal_uint32 sensor_id = 0;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 5;
    }
	//s5kgw1spCheckLensVersion(imgsensor.i2c_write_id);
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;
    /* initail sequence write in  */
    sensor_init();
    // s5kgw1sp_MIPI_update_wb_register_from_otp();
    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = KAL_FALSE;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);
    //flag = read_cmos_sensor_16_16(0x05);  
   // LOG_INF("Read sensor id 0x05, 0x%x\n", flag);    
    return ERROR_NONE;
}   /*  open  */
/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*	
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/*No Need to implement this function*/ 
	return ERROR_NONE;
}	/*	close  */
/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength; 
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(IMAGE_NORMAL);	
	mdelay(10);
	return ERROR_NONE;
}	/*	preview   */
/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    // int i;
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	// if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		LOG_INF("capture30fps: use cap30FPS's setting: %d fps!\n",imgsensor.current_fps/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	// } 
	// else  
	// if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
	// 	LOG_INF("cap115fps: use cap1's setting: %d fps!\n",imgsensor.current_fps/10);
	// 	imgsensor.pclk = imgsensor_info.cap1.pclk;
	// 	imgsensor.line_length = imgsensor_info.cap1.linelength;
	// 	imgsensor.frame_length = imgsensor_info.cap1.framelength;  
	// 	imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
	// 	imgsensor.autoflicker_en = KAL_FALSE;
	// }
	// else  { //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
	// 	LOG_INF("Warning:=== current_fps %d fps is not support, so use cap1's setting\n",imgsensor.current_fps/10);
	// 	imgsensor.pclk = imgsensor_info.cap1.pclk;
	// 	imgsensor.line_length = imgsensor_info.cap1.linelength;
	// 	imgsensor.frame_length = imgsensor_info.cap1.framelength;  
	// 	imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
	// 	imgsensor.autoflicker_en = KAL_FALSE;
	// }
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps); 
	set_mirror_flip(IMAGE_NORMAL);	
	mdelay(10);
	// for(i=0; i<10; i++){
	// 	LOG_INF("delay time = %d, the frame no = %d\n", i*10, read_cmos_sensor(0x0005));
	// 	mdelay(10);
	// }
#if 0
	if(imgsensor.test_pattern == KAL_TRUE)
	{
		//write_cmos_sensor(0x5002,0x00);
  }
#endif
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(IMAGE_NORMAL);	
	return ERROR_NONE;
}	/*	normal_video   */
static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	hs_video   */
static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}
/*************************************************************************
* FUNCTION
* Custom1
*
* DESCRIPTION
*   This function start the sensor Custom1.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength; 
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom1   */
#if 0
static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength; 
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom2   */
static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength; 
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom3   */
static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
    imgsensor.pclk = imgsensor_info.custom4.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom4.linelength;
    imgsensor.frame_length = imgsensor_info.custom4.framelength; 
    imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom4   */
static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
    imgsensor.pclk = imgsensor_info.custom5.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom5.linelength;
    imgsensor.frame_length = imgsensor_info.custom5.framelength; 
    imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom5   */
#endif
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;		
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
    sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;
    /*sensor_resolution->SensorCustom2Width  = imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height     = imgsensor_info.custom2.grabwindow_height;
    sensor_resolution->SensorCustom3Width  = imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height     = imgsensor_info.custom3.grabwindow_height;
    sensor_resolution->SensorCustom4Width  = imgsensor_info.custom4.grabwindow_width;
    sensor_resolution->SensorCustom4Height     = imgsensor_info.custom4.grabwindow_height;
    sensor_resolution->SensorCustom5Width  = imgsensor_info.custom5.grabwindow_width;
    sensor_resolution->SensorCustom5Height     = imgsensor_info.custom5.grabwindow_height;*/
	return ERROR_NONE;
}	/*	get_resolution	*/
static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */
	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;
	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame; 
   /* sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame; 
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame; 
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame; 
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame; */
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;	
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num; 
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
	sensor_info->SensorPacketECCOrder = 1;
	#ifdef FPTPDAFSUPPORT
	sensor_info->PDAF_Support = 2;
	#else 
	sensor_info->PDAF_Support = 0;
	#endif
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc; 
			break;	 
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc; 
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:			
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc; 
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc; 
			break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;   
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 
            break;
       /* case MSDK_SCENARIO_ID_CUSTOM2:
            sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;   
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;   
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;   
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx; 
            sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;   
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc; 
            break;*/
		default:			
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	return ERROR_NONE;
}	/*	get_info  */
static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;	
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;	  
        case MSDK_SCENARIO_ID_CUSTOM1:
            Custom1(image_window, sensor_config_data); // Custom1
            break;
        /*case MSDK_SCENARIO_ID_CUSTOM2:
            Custom2(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            Custom3(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            Custom4(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            Custom5(image_window, sensor_config_data); // Custom1
			break;	  */
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */
static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);
	return ERROR_NONE;
}
static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker	  
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
{
	kal_uint32 frame_length;
	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;			
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			// if(framerate==300)
			// {
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			// }
			// else
			// {
			// frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			// spin_lock(&imgsensor_drv_lock);
			// imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
			// imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			// imgsensor.min_frame_length = imgsensor.frame_length;
			// spin_unlock(&imgsensor_drv_lock);
			// }
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;	
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            break;
       /* case MSDK_SCENARIO_ID_CUSTOM2:
            frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            break; 
        case MSDK_SCENARIO_ID_CUSTOM3:
            frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            break; 
        case MSDK_SCENARIO_ID_CUSTOM4:
            frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            break; 
        case MSDK_SCENARIO_ID_CUSTOM5:
            frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ? (frame_length - imgsensor_info.custom5.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			break;		*/
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}	
	return ERROR_NONE;
}
static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;		
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO: 
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *framerate = imgsensor_info.custom1.max_framerate;
            break;
        /*case MSDK_SCENARIO_ID_CUSTOM2:
            *framerate = imgsensor_info.custom2.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *framerate = imgsensor_info.custom3.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            *framerate = imgsensor_info.custom4.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *framerate = imgsensor_info.custom5.max_framerate;
            break;*/
		default:
			break;
	}
	return ERROR_NONE;
}
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	if (enable) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		 write_cmos_sensor(0x6028, 0x2000);
		 write_cmos_sensor(0x602A, 0x1082);
		 write_cmos_sensor(0x6F12, 0x0000);
		 write_cmos_sensor(0x3734, 0x0001);
		 write_cmos_sensor(0x0600, 0x0308);
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		 write_cmos_sensor(0x6028, 0x2000);
		 write_cmos_sensor(0x602A, 0x1082);
		 write_cmos_sensor(0x6F12, 0x8010);
		 write_cmos_sensor(0x3734, 0x0010);
		 write_cmos_sensor(0x0600, 0x0300);
	}	 
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;    
	UINT32 fps = 0;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;


	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 4;
			break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		pr_debug("imgsensor.pclk = %d,current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
			break;
	//[agold][yz] add for ERROR: [0x1]:lineT | frameT can't be 0
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= imgsensor_info.pre.pclk;
			break;
		}
		break;
	//[agold][yz] add for ERROR: [0x1]:lineT | frameT can't be 0
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
			break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    /*night_mode((BOOL) *feature_data); no need to implement this mode*/
			break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
			break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
	case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(
			    sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
	case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */

		/* if EEPROM does not exist in camera module.*/
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(
			(BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
		  (enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;

		/*for factory mode auto testing*/
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.hdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

		wininfo =
	    (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));

		/* ihdr_write_shutter_gain((UINT16)*feature_data,
		 * (UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		/* ihdr_write_shutter(
		 * (UINT16)*feature_data,(UINT16)*(feature_data+1));
		 */
		break;
		/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16)*feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: /*full*/
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW: /*2x2 binning*/
			memcpy(
				(void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
				break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
		  "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16)*feature_data);
		/*PDAF capacity enable or not*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting*/
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			/*need to check*/
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
			pr_debug("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode = *feature_data_16;
			break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
			set_shutter_frame_length((UINT16)*feature_data,
					(UINT16)*(feature_data+1));
			break;
	/******************** STREAMING RESUME/SUSPEND *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (MUINT32)(*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			// if (fps == 240)
			// *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			// 	imgsensor_info.cap1.mipi_pixel_rate;
			// else if (fps == 150)
			// *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			// 	imgsensor_info.cap2.mipi_pixel_rate;
			// else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}

		break;

	default:
			break;
	}
    return ERROR_NONE;
}    /*    feature_control()  */
static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KGW1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	S5Kgw1_MIPI_RAW_SensorInit	*/
