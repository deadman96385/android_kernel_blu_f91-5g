/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * BU6429AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "BU6429AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

/* if use ISRC mode, should modify variables in init_setting */


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;
static long g_i4MotorStatus;
static long g_i4Dir;
static int g_sr = 3;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;
//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-begin
static unsigned long AFInf_u4CurrPosition = 0;
//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-end
static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16) pBuff[0]) << 2) + (pBuff[1]);

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[3] = { 0x03, (char)(a_u2Data >> 8), (char)(a_u2Data & 0xFF) };

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static int gt9767_init(void)
{
	int i4RetValue = 0;
	char puSendCmd[3];
   
	printk("[GT9767AF] gt9767_init - beg\n");
	puSendCmd[0] = 0x02;
	puSendCmd[1] = 0x02;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("bu6429 101 [GT9767AF] I2C send failed!! \n");
	}
	mdelay(5);
	puSendCmd[0] = 0x06;
	puSendCmd[1] = 0x41;   //ZVDD mode
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("bu6429 109 [GT9767AF] I2C send failed!! \n");
	}
	puSendCmd[0] = 0x07;
	puSendCmd[1] = 0x39;   //ZVDD mode
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   printk("bu6429 116 [GT9767AF] I2C send failed!! \n");
	}
	
	
	/*puSendCmd[0] = 0x03;
	puSendCmd[1] = 0x00; //T_SRC[4:0]=00000(default)
	puSendCmd[2] = 0x19;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
	if (i4RetValue < 0)
	{
	   printk("[GT9800AF] I2C send failed!! \n");
	}*/
	
	LOG_INF("[GT9767AF] GT9767AF SRC Init End!! \n");
   
	return i4RetValue;
}


static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	if (g_i4MotorStatus == 1)
		stMotorInfo.bIsMotorMoving = 1;
	else
		stMotorInfo.bIsMotorMoving = 0;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		printk("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;
		//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-begin
		AFInf_u4CurrPosition = a_u4Position;
		//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-end
		gt9767_init();

		ret = s4AF_ReadReg(&InitPos);
//printk("GT9767AF read InitPos:%d ret: %d \n", InitPos, ret);
		if (ret == 0) {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			//prize modify by yantaotao for crash noise start
			g_u4CurrPosition = 508;
			//prize modify by yantaotao for crash noise end
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(g_pAF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(g_pAF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	printk("GT9767AF move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

	spin_lock(g_pAF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(g_pAF_SpinLock);

	//modify by yantaotao for AF 2021-10-20 start 
	if(g_u4TargetPosition > g_u4CurrPosition) {
		while(g_u4TargetPosition - g_u4CurrPosition > 50) {
			g_u4CurrPosition += 50;
			if (s4AF_WriteReg((unsigned short)g_u4CurrPosition) == 0) {
				spin_lock(g_pAF_SpinLock);			
				spin_unlock(g_pAF_SpinLock);
			} else {
				printk("set I2C failed when moving the motor\n");

				spin_lock(g_pAF_SpinLock);
				g_i4MotorStatus = -1;
				spin_unlock(g_pAF_SpinLock);
			}
			msleep(15);
		}
	}
	
	if(g_u4TargetPosition < g_u4CurrPosition) {
		while(g_u4CurrPosition - g_u4TargetPosition > 150) {
			if (s4AF_WriteReg((unsigned short)g_u4CurrPosition - 150) == 0) {
				spin_lock(g_pAF_SpinLock);
				g_u4CurrPosition -= 150;
				spin_unlock(g_pAF_SpinLock);
			} else {
				printk("set I2C failed when moving the motor\n");

				spin_lock(g_pAF_SpinLock);
				g_i4MotorStatus = -1;
				spin_unlock(g_pAF_SpinLock);
			}
			msleep(15);
		}
	}
	//modify by yantaotao for AF 2021-10-20 end 

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		printk("set I2C failed when moving the motor\n");

		spin_lock(g_pAF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(g_pAF_SpinLock);
	}

	return 0;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long BU6429AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *) (a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int BU6429AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	printk("Start\n");
printk("GT9767AF %s, g_pAF_Opened:%d, AFInf_u4CurrPosition:%d, g_u4CurrPosition:%d ===yantt \n",__func__, *g_pAF_Opened, AFInf_u4CurrPosition, g_u4CurrPosition);
	if (*g_pAF_Opened == 2) {
		//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-begin
		#if 0
		LOG_INF("Wait\n");
		s4AF_WriteReg(200);
		msleep(20);
		s4AF_WriteReg(100);
		msleep(20);
		#endif
		
	//modify by yantaotao for AF 2021-10-20 start 		
		int ret = 0;
		unsigned long af_step = 50;
		unsigned long nextPosition = 0;
		//unsigned long endPos = 512;
		// >512 multiple times
		// first step move fast
		if (g_u4CurrPosition > AFInf_u4CurrPosition) {
			if (g_u4CurrPosition > 800) {
				nextPosition = 800;
				LOG_INF("0:nextPosition = %d g_u4CurrPosition = %d g_u4AF_MACRO = %d",
					nextPosition, g_u4CurrPosition,g_u4AF_MACRO);
				if (s4AF_WriteReg((unsigned short)nextPosition) == 0) {
					g_u4CurrPosition = nextPosition;
					ret = 0;
				} else {
					LOG_INF("set I2C failed when moving the motor\n");
					ret = -1;
				}
			}
			mdelay(10);
			while (g_u4CurrPosition > AFInf_u4CurrPosition + af_step) {
				if (g_u4CurrPosition > 700) {
					af_step = 50;
				}
				nextPosition = g_u4CurrPosition - af_step;
				LOG_INF("1:af_step = %d g_u4CurrPosition = %d", af_step,
					g_u4CurrPosition);
				if (s4AF_WriteReg((unsigned short)nextPosition) == 0) {
					g_u4CurrPosition = nextPosition;
					ret = 0;
				} else {
					LOG_INF("set I2C failed when moving the motor\n");
					ret = -1;
					break;
				}
				mdelay(10);
			}
		} else if (g_u4CurrPosition < AFInf_u4CurrPosition) {
			// <512 multiple times
			// first step move fast
			if (g_u4CurrPosition < 300) {
				nextPosition = 300;
				LOG_INF("0:nextPosition = %d g_u4CurrPosition = %d g_u4AF_MACRO = %d",
				nextPosition, g_u4CurrPosition,g_u4AF_MACRO);
				if (s4AF_WriteReg((unsigned short)nextPosition) == 0) {
					g_u4CurrPosition = nextPosition;
					ret = 0;
				} else {
					LOG_INF("set I2C failed when moving the motor\n");
					ret = -1;
				}
			}
			mdelay(10);
			while (g_u4CurrPosition < AFInf_u4CurrPosition - af_step) {
				if (g_u4CurrPosition > 400) {
					af_step = 50;
				}
				nextPosition = g_u4CurrPosition + af_step;
				LOG_INF("1:af_step = %d g_u4CurrPosition = %d", af_step,
				g_u4CurrPosition);
				if (s4AF_WriteReg((unsigned short)nextPosition) == 0) {
					g_u4CurrPosition = nextPosition;
					ret = 0;
				} else {
					LOG_INF("set I2C failed when moving the motor\n");
					ret = -1;
					break;
				}
				mdelay(10);
			}
		}
		s4AF_WriteReg(AFInf_u4CurrPosition);
		//modify by yantaotao for AF 2021-10-20 end 
		//prize-camera  add for remove motor crash noise by zhuzhengjiang 20191101-end
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int BU6429AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}

int BU6429AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
