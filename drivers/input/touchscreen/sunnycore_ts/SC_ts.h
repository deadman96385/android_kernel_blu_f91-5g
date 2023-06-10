/* 
 * Goodix GT9xx touchscreen driver
 * 
 * Copyright  (C)  2010 - 2014 Goodix. Ltd.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be a reference 
 * to you, when you are integrating the GOODiX's CTP IC into your system, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 * 
 * Version: 2.4
 * Release Date: 2014/11/28
 */

#ifndef __SC_TS_H__
#define __SC_TS_H__

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "SC_chip_common.h"
struct SC_ts_data {
    spinlock_t irq_lock;
    struct i2c_client *client;
    struct input_dev  *input_dev;
    struct hrtimer timer;
    struct work_struct  work;
    s32 irq_is_disable;
    s32 use_irq;
    u16 abs_x_max;
    u16 abs_y_max;
    u8  max_touch_num;
    u8  int_trigger_type;
    u8  enter_update;
    u8  SC_is_suspend;

#if defined(CONFIG_FB)
	struct notifier_block notifier;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

// STEP_2(REQUIRED): Customize your I/O ports & I/O operations
#define SC_RST_PORT    16//S5PV210_GPJ3(6)
#define SC_INT_PORT    17//S5PV210_GPH1(3)

#define SC_GPIO_AS_INPUT(pin)          do{\
                                            gpio_direction_input(pin);\
                                        }while(0)
#define SC_GPIO_AS_INT(pin)            do{\
                                            SC_GPIO_AS_INPUT(pin);\
                                        }while(0)
#define SC_GPIO_GET_VALUE(pin)         gpio_get_value(pin)
#define SC_GPIO_OUTPUT(pin,level)      gpio_direction_output(pin,level)
#define SC_GPIO_REQUEST(pin, label)    gpio_request(pin, label)
#define SC_GPIO_FREE(pin)              gpio_free(pin)
#define SC_IRQ_TAB                     {IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING, IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH}

//***************************PART3:OTHER define*********************************
#define SC_DRIVER_VERSION          "V2.4<2020/15/01>"
#define SC_I2C_NAME                "SunnyCore_ts"
#define SC_POLL_TIME         10    

extern struct i2c_client * SC_i2c_client;
extern int SC_i2c_write(struct i2c_client* client, unsigned char addr, unsigned char* writebuf, int writelen);
extern int SC_i2c_read(struct i2c_client* client, unsigned char addr, unsigned char* readbuf, int readlen);
extern int SC_get_chip_id(unsigned char *buf);
extern int SC_get_fwArgPrj_id(unsigned char *buf);
extern int SC_get_cob_id(unsigned char *buf);
extern int SC_get_prj_id(unsigned char *buf);
#ifdef SC_AUTO_UPDATE_FARMWARE
extern int SC_auto_update_fw(void);
#endif
#ifdef SC_UPDATE_FARMWARE_WITH_BIN
extern int SC_fw_upgrade_with_bin_file(unsigned char *firmware_path);
#endif
extern int SC_get_chip_id(unsigned char *buf);
void SC_ts_set_intup(char level);
extern int SC_power_switch(struct i2c_client *client, int on);
#ifdef  RESET_PIN_WAKEUP
extern void SC_ts_reset_wakeup(void);
#endif
#endif
