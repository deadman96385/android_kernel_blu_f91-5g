/* 
 * touchscreen driver
 * 
 * Copyright  (C)  2010 - 2014 . Ltd.
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
 * Release Date:
 */

#include "SC_ts.h"

static const char *SC_ts_name = "SunnyCore-ts";
static const char *SC_input_phys = "input/ts";
static struct workqueue_struct *SC_wq;
struct i2c_client * SC_i2c_client = NULL; 
struct mutex SC_i2c_lock;  

int SC_rst_gpio;
int SC_int_gpio;
static int SC_register_powermanger(struct SC_ts_data *ts);
static int SC_unregister_powermanger(struct SC_ts_data *ts);

int SC_i2c_write_read(struct i2c_client* client, unsigned char addr, unsigned char* writebuf, int writelen, unsigned char* readbuf, int readlen)
{
    int ret = 0;

    if(readlen > 0) {
        if(writelen > 0) {
            struct i2c_msg msgs[] = {
                {
                    .addr = addr,
                    .flags = 0,
                    .len = writelen,
                    .buf = writebuf,
		    //liu
                    //.scl_rate = 200 * 1000, 
                },
                {
                    .addr = client->addr,
                    .flags = I2C_M_RD,
                    .len = readlen,
                    .buf = readbuf,
		    //liu
                    //.scl_rate = 200 * 1000, 
                },
            };

            ret = i2c_transfer(client->adapter, msgs, 2) == 2 ? 0 : -1;
            if(ret < 0)
                SC_DEBUG("%s: sunnycorei2c write-read error.", readbuf);

        } else {
            struct i2c_msg msgs[] = {
                {
                    .addr = addr,
                    .flags = I2C_M_RD,
                    .len = readlen,
                    .buf = readbuf,
		    //liu
                    //.scl_rate = 200 * 1000, 
                },
            };

            ret = i2c_transfer(client->adapter, msgs, 1) == 1 ? 0 : -1;
            if(ret < 0)
                SC_DEBUG("%s: sunnycore i2c read error.", readbuf);
        }
    }

    return ret;
}

int SC_i2c_write(struct i2c_client* client, unsigned char addr, unsigned char* writebuf, int writelen)
{
    int ret = 0;

    struct i2c_msg msgs[] = {
        {
            .addr = addr,
            .flags = 0,
            .len = writelen,
            .buf = writebuf,
			//.timing = 200,
	    //liu
            //.scl_rate = 200 * 1000, 
        },
    };

    if(writelen > 0) {
        ret = i2c_transfer(client->adapter, msgs, 1) == 1 ? 0 : -1;
        if(ret < 0) {
            int i;
            SC_DEBUG("lqw 33 sunnycore i2c write error.\n");
            for (i = 0; i < writelen; i++)
                SC_DEBUG("sunnycore i2c write 0x%02x error.\n", writebuf[i]);
        }
    }

    return ret;
}

int SC_i2c_read(struct i2c_client* client, unsigned char addr, unsigned char* readbuf, int readlen)
{
    int ret = 0;

    struct i2c_msg msgs[] = {
        {
            .addr = addr,
            .flags = I2C_M_RD,
            .len = readlen,
            .buf = readbuf,
			//.timing = 200,
	    //liu
            //.scl_rate = 20 * 1000,
        },
    };

    ret = i2c_transfer(client->adapter, msgs, 1) == 1 ? 0 : -1;
    if(ret < 0)
        SC_DEBUG("sunnycore i2c read error.");
			
    return ret;
}

void SC_i2c_lock_enable(int enable)
{
    if(enable)
        mutex_lock(&SC_i2c_lock);
    else
        mutex_unlock(&SC_i2c_lock);
}

#if defined(CONFIG_TPD_ROTATE_90) || defined(CONFIG_TPD_ROTATE_270)
static void tpd_swap_xy(int* x, int* y)
{
    int temp = 0;

    temp = *x;
    *x = *y;
    *y = temp;
}
#endif

#if defined(CONFIG_TPD_ROTATE_90)

static void tpd_rotate_90(int* x, int* y)
{
    int temp;
    temp = *y;
	*y = TPD_RES_X - *x;
	*x = *y;
}

#endif

#if defined(CONFIG_TPD_ROTATE_180)
static void tpd_rotate_180(int* x, int* y)
{
    *y = TPD_RES_Y + 1 - *y;
    *x = TPD_RES_X + 1 - *x;
}
#endif

#if defined(CONFIG_TPD_ROTATE_X_180)
static void tpd_rotate_x_180(int* x, int* y)
{
    *x = TPD_RES_X + 1 - *x;
}
#endif

#if defined(CONFIG_TPD_ROTATE_270)
static void tpd_rotate_270(int* x, int* y)
{
    int temp;
    temp = *x;
    *x = TPD_RES_Y - *y;
    *y = temp;
}
#endif//CONFIG_TPD_ROTATE_XXX

void SC_ts_set_intmode(char mode)
{
	if(0 == mode)
	{
		SC_GPIO_OUTPUT(SC_int_gpio, 1);
	}
	else if(1 == mode)
	{
	    SC_GPIO_AS_INT(SC_int_gpio);	
	}
}

void SC_ts_set_intup(char level)
{
	if(level==1)
		SC_GPIO_OUTPUT(SC_int_gpio, 1);
	else if(level==0)
		SC_GPIO_OUTPUT(SC_int_gpio, 0);
}

#ifdef INT_PIN_WAKEUP
void SC_ts_int_wakeup(void)
{
	SC_GPIO_OUTPUT(SC_int_gpio, 1);
	msleep(20);
	SC_GPIO_OUTPUT(SC_int_gpio, 0);
	msleep(1);
	SC_GPIO_OUTPUT(SC_int_gpio, 1);
	msleep(20);
}
#endif

#ifdef  RESET_PIN_WAKEUP
void SC_ts_reset_wakeup(void)
{
	SC_GPIO_OUTPUT(SC_rst_gpio, 1);
	msleep(20);
	SC_GPIO_OUTPUT(SC_rst_gpio, 0);
	msleep(20);
	SC_GPIO_OUTPUT(SC_rst_gpio, 1);
	msleep(20);
}
#endif

#if 0//defined(SC_AUTO_UPDATE_FARMWARE)
static u8 SC_init_update_proc(struct SC_ts_data *ts)
{
    struct task_struct *thread = NULL;

    SC_INFO("Ready to run update thread.");
  
    thread = kthread_run(SC_auto_update_fw, (void*)NULL, "SC_update");
	
    if (IS_ERR(thread))
    {
        SC_ERROR("Failed to create update thread.\n");
        return -1;
    }

    return 0;
}
#endif

/*******************************************************
Function:
    Disable irq function
Input:
    ts: goodix i2c_client private data
Output:
    None.
*********************************************************/
void SC_irq_disable(struct SC_ts_data *ts)
{
    unsigned long irqflags;

    SC_DEBUG_FUNC();

    spin_lock_irqsave(&ts->irq_lock, irqflags);
    if (!ts->irq_is_disable)
    {
        ts->irq_is_disable = 1; 
        disable_irq_nosync(ts->client->irq);
    }
    spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

/*******************************************************
Function:
    Enable irq function
Input:
    ts: goodix i2c_client private data
Output:
    None.
*********************************************************/
void SC_irq_enable(struct SC_ts_data *ts)
{
    unsigned long irqflags = 0;

    SC_DEBUG_FUNC();
    
    spin_lock_irqsave(&ts->irq_lock, irqflags);
    if (ts->irq_is_disable) 
    {
        enable_irq(ts->client->irq);
        ts->irq_is_disable = 0; 
    }
    spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static ssize_t SC_fwVersion_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    unsigned char fwVer[3] = {0x00, 0x00, 0x00};//依次为固件算法版本、参数版本、客户ID
	int ret = 0;

	ret = SC_get_fwArgPrj_id(fwVer);
	return sprintf(buf, "fwVer:%d\nargVer:%d\nPrjId:%d\n",fwVer[0],fwVer[1],fwVer[2]);

	if(ret<0)
	return sprintf(buf, "%s\n", "Read Version FAIL");
}
static DEVICE_ATTR(fwVersion, 0664, SC_fwVersion_show, NULL);


#ifdef SC_DEBUGFS_SUPPORT

static ssize_t SC_chipID_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    unsigned char chipID = 0x00;
	int ret = 0;

	struct SC_ts_data *ts = i2c_get_clientdata(SC_i2c_client);;
    
	SC_irq_disable(ts);
	SC_ts_set_intmode(0);
	ret = SC_get_chip_id(&chipID);
	SC_ts_set_intmode(1);
	SC_irq_enable(ts);
	if(ret<0)
		return sprintf(buf, "%s\n", "Read ChipID FAIL");
	else
		return sprintf(buf, "ChipID = %d\n", chipID);
}
static DEVICE_ATTR(chipID, 0664, SC_chipID_show, NULL);

static ssize_t SC_cobID_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    unsigned char cobID[6] = {0x00};
	int ret = 0;

    ret = SC_get_cob_id(cobID);
	if(ret<0)
		return sprintf(buf, "%s\n", "Read cobID FAIL");
	else
		return sprintf(buf, "cobID = %x:%x:%x:%x:%x:%x\n", cobID[0],cobID[1],cobID[2],cobID[3],cobID[4],cobID[5]);
}
static DEVICE_ATTR(cobID,0664, SC_cobID_show, NULL);
#ifdef SC_UPDATE_FARMWARE_WITH_BIN
static ssize_t SC_fwUpdate_store(struct device *dev,struct device_attribute *attr, char *buf)
{
	int ret = 0;

	struct SC_ts_data *ts = i2c_get_clientdata(SC_i2c_client);;

	ret = strcmp(buf,"update");
	if(ret)
    {
        return sprintf(buf, "%s\n", "Please write the command:echo \"update > fwUpdate\" in the console!\n");
    }
	else
    {
        SC_i2c_lock_enable(1);
        SC_irq_disable(ts);
        SC_ts_set_intmode(0);
        ret = SC_fw_upgrade_with_bin_file(SC_FIRMWARE_BIN_PATH);
        SC_ts_set_intmode(1);
        SC_irq_enable(ts);   
		SC_i2c_lock_enable(0;);
    }

	if(ret<0)
		return sprintf(buf, "%s\n", "Update firmware bin FAIL");
	else
		return sprintf(buf, "%s\n", "Update firmware bin SUCESS");
}

static DEVICE_ATTR(fwUpdate,0664, NULL, SC_fwUpdate_store);
#endif

static struct attribute *SC_debug_attrs[] = {
	&dev_attr_fwVersion.attr,
    &dev_attr_chipID.attr,
    &dev_attr_cobID.attr,
    #ifdef SC_UPDATE_FARMWARE_WITH_BIN
    &dev_attr_fwUpdate.attr,
    #endif
    NULL,
};
static const struct attribute_group SC_debug_attr_group = {
	.attrs = SC_debug_attrs,
	.name = "SC_debug",
};
#endif

/*******************************************************
Function:
    Report touch point event 
Input:
    ts: goodix i2c_client private data
    id: trackId
    x:  input x coordinate
    y:  input y coordinate
    w:  input pressure
Output:
    None.
*********************************************************/
static void SC_touch_down(struct SC_ts_data* ts,s32 id,s32 x,s32 y,s32 w, s8 pressure)
{
	//SC_DEBUG("SC_touch_down:X_origin:%d, Y_origin:%d", x, y);

    #if defined(CONFIG_TPD_ROTATE_90)
    		tpd_rotate_90(&x, &y);
    #elif defined(CONFIG_TPD_ROTATE_270)
    		tpd_rotate_270(&x, &y);
    #elif defined(CONFIG_TPD_ROTATE_180)
    		tpd_rotate_180(&x, &y);
    #elif defined(CONFIG_TPD_ROTATE_X_180)
    		tpd_rotate_x_180(&x, &y);
    #endif

#ifdef SC_VIRTRUAL_KEY_SUPPORT
    if(x == SC_VIRTRUAL_KEY1_X && y == SC_VIRTRUAL_KEY1_Y) {  
    	input_report_key(ts->input_dev, SC_VIRTRUAL_KEY1, 1);
    	input_sync(ts->input_dev);
    	return;
    }
    
    if(x == SC_VIRTRUAL_KEY2_X && y == SC_VIRTRUAL_KEY2_Y) {  
    	input_report_key(ts->input_dev, SC_VIRTRUAL_KEY2, 1);
    	input_sync(ts->input_dev);
    	return;
    }
    
    if(x == SC_VIRTRUAL_KEY3_X && y == SC_VIRTRUAL_KEY3_Y) {  
    	input_report_key(ts->input_dev, SC_VIRTRUAL_KEY3, 1);
    	input_sync(ts->input_dev);
    	return;
    }
#endif

#ifdef SC_CTP_SUPPORT_TYPEB_PROTOCOL
	input_mt_slot(ts->input_dev, id);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
#else
    input_report_key(ts->input_dev, BTN_TOUCH, 1);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
    input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
    #ifdef SC_CTP_PRESSURE
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
	#endif
    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
    input_mt_sync(ts->input_dev);
#endif
    SC_DEBUG("SC_touch_down:ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

/*******************************************************
Function:
    Report touch release event
Input:
    ts: goodix i2c_client private data
Output:
    None.
*********************************************************/
static void SC_touch_up(struct SC_ts_data* ts, s32 id)
{
#ifdef SC_VIRTRUAL_KEY_SUPPORT
		if(x == SC_VIRTRUAL_KEY1_X && y == SC_VIRTRUAL_KEY1_Y) { 
			input_report_key(ts->input_dev, SC_VIRTRUAL_KEY1, 0);
			input_sync(ts->input_dev);
			return;
		}
		
		if(x == SC_VIRTRUAL_KEY2_X && y == SC_VIRTRUAL_KEY2_Y) { 
			input_report_key(ts->input_dev, SC_VIRTRUAL_KEY2, 0);
			input_sync(ts->input_dev);
			return;
		}
		
		if(x == SC_VIRTRUAL_KEY3_X && y == SC_VIRTRUAL_KEY3_Y) {
			input_report_key(ts->input_dev, SC_VIRTRUAL_KEY3, 0);
			input_sync(ts->input_dev);
			return;
		}
#endif

#ifdef SC_CTP_SUPPORT_TYPEB_PROTOCOL
    input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#else
    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
    #ifdef SC_CTP_PRESSURE
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
    #endif
	input_mt_sync(ts->input_dev);
#endif
	//SC_DEBUG("SC_touch_up:ID:%d", id);
}

//
/*
static void SC_touch_release(struct SC_ts_data* ts, s32 id)
{
#ifdef SC_CTP_SUPPORT_TYPEB_PROTOCOL
    input_mt_slot(ts->input_dev, id);
    input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);   
#endif
}
*/
/*******************************************************
Function:
    Goodix touchscreen work function
Input:
    work: work struct of goodix_workqueue
Output:
    None.
*********************************************************/
static void SC_ts_work_func(struct work_struct *work)
{
	u8  cmd = TS_DATA_REG;
	u8  cmd_second = TS_DATA_REG+1;
    u8  point_data[8 * MAX_POINT_NUM]={0};
    u8  touch_num = 0;
	u8  pressure = 0x0;
    s32 input_x = 0;
    s32 input_y = 0;
    s32 id = 0;
    u8  eventFlag = 0;
    s32 i  = 0;
    s32 ret = -1;
    struct SC_ts_data *ts = NULL;

    SC_DEBUG_FUNC();
	
    ts = container_of(work, struct SC_ts_data, work);
    if (ts->enter_update)
    {
        return;
    }

    ret = SC_i2c_write_read(SC_i2c_client, SC_i2c_client->addr, &cmd, 1, point_data, 8);
	touch_num = point_data[7];
	if(touch_num>5)
	 {
	     touch_num=5;
	 }
	
	if(touch_num)
	{
    	ret = SC_i2c_write_read(SC_i2c_client, SC_i2c_client->addr, &cmd_second, 1, point_data + 8, 8 * touch_num);
	}
    if (ret < 0)
    {
        SC_ERROR("I2C transfer error. errno:%d\n ", ret);
        if (ts->use_irq)
        {
            SC_irq_enable(ts);
        }
        return;
    }
    
    touch_num += 1;
	
    if (touch_num > 5)
    {
        goto exit_work_func;
    }

    #ifdef SC_CTP_PRESSURE
	pressure = point_data[7];
    #endif

	for (i = 0; i < 5; i++)
	{
		eventFlag = point_data[8 * i]; 	        
		id = point_data[1 + 8 * i]&0x0F;
		input_x  = ((point_data[2 + 8 * i] & 0xff) << 4) | ((point_data[4 + 8 * i]&0xf0)>>4);
		input_y  = ((point_data[3 + 8 * i] & 0xff) << 4) | (point_data[4 + 8 * i]&0x0f);
		
		//SC_DEBUG("%s:eventFlag:%d, id:%d, x:%d, y:%d", __func__,eventFlag,id, input_x, input_y);
	    if((id!=0x0f)&&((eventFlag == CTP_MOVE) || (eventFlag == CTP_DOWN)))
        {
            SC_touch_down(ts, id, input_x, input_y, 1, pressure);
        }
		else if(eventFlag == CTP_UP)
        {
            SC_touch_up(ts, id);
        }
	}

    input_sync(ts->input_dev);

exit_work_func:
    if (ts->use_irq)
    {
        SC_irq_enable(ts);
    }
}

/*******************************************************
Function:
    Timer interrupt service routine for polling mode.
Input:
    timer: timer struct pointer
Output:
    Timer work mode. 
        HRTIMER_NORESTART: no restart mode
*********************************************************/
static enum hrtimer_restart SC_ts_timer_handler(struct hrtimer *timer)
{
    struct SC_ts_data *ts = container_of(timer, struct SC_ts_data, timer);

    SC_DEBUG_FUNC();

    queue_work(SC_wq, &ts->work);
    hrtimer_start(&ts->timer, ktime_set(0, (SC_POLL_TIME+6)*1000000), HRTIMER_MODE_REL);
    return HRTIMER_NORESTART;
}

/*******************************************************
Function:
    External interrupt service routine for interrupt mode.
Input:
    irq:  interrupt number.
    dev_id: private data pointer
Output:
    Handle Result.
        IRQ_HANDLED: interrupt handled successfully
*********************************************************/
static irqreturn_t SC_ts_irq_handler(int irq, void *dev_id)
{
    struct SC_ts_data *ts = dev_id;

    SC_DEBUG_FUNC();
 
    SC_irq_disable(ts);

    queue_work(SC_wq, &ts->work);
    
    return IRQ_HANDLED;
}


/*******************************************************
Function:
    Enter sleep mode.
Input:
    ts: private data.
Output:
    Executive outcomes.
       1: succeed, otherwise failed.
*******************************************************/
static s8 SC_enter_sleep(struct SC_ts_data * ts)
{
    s8 ret = -1;
    unsigned char cmd[]={LPM_REG,0x03};
    SC_DEBUG_FUNC();
    
    
    ret = SC_i2c_write(ts->client, CTP_SLAVE_ADDR, cmd, 2);
    if (ret == 0)
    {
        SC_INFO("SC enter sleep!");
        
        return ret;
    }
    msleep(10);
	
    SC_ERROR("SC send sleep cmd failed.");
    return ret;
}
/*******************************************************
Function:
    Wakeup from sleep.
Input:
    ts: private data.
Output:
    Executive outcomes.
        >0: succeed, otherwise: failed.
*******************************************************/
static s8 SC_wakeup_sleep(struct SC_ts_data * ts)
{
    s8 ret = -1;
    unsigned char fwArgu[2] = {0};
    SC_DEBUG_FUNC();


    #if defined(RESET_PIN_WAKEUP)
        SC_ts_reset_wakeup();
    #endif
    #if defined(INT_PIN_WAKEUP)
        SC_ts_int_wakeup();
        SC_ts_set_intmode(1);
    #endif
    
    ret = SC_get_fwArgPrj_id(fwArgu);
    if (ret == 0)
    {
        SC_INFO("SC wakeup sleep.");            
        return ret;
    }

    SC_ERROR("SC wakeup sleep failed.");
    return ret;
}

/*******************************************************
Function:
    Request gpio(INT & RST) ports.
Input:
    ts: private data.
Output:
    Executive outcomes.
        >= 0: succeed, < 0: failed
*******************************************************/
static s8 SC_request_io_port(struct SC_ts_data *ts)
{
    s32 ret = 0;

    SC_DEBUG_FUNC();
    ret = SC_GPIO_REQUEST(SC_int_gpio, "SC INT IRQ");
    if (ret < 0) 
    {
        SC_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32)SC_int_gpio, ret);
        ret = -ENODEV;
    }
    else
    {
        SC_GPIO_AS_INT(SC_int_gpio);  
        ts->client->irq = gpio_to_irq(SC_int_gpio);
    }

    ret = SC_GPIO_REQUEST(SC_rst_gpio, "SC RST PORT");
    if (ret < 0) 
    {
        SC_ERROR("Failed to request GPIO:%d, ERRNO:%d",(s32)SC_rst_gpio,ret);
        ret = -ENODEV;
    }

    SC_GPIO_AS_INPUT(SC_rst_gpio);
    
    if(ret < 0)
    {
        SC_GPIO_FREE(SC_rst_gpio);
        SC_GPIO_FREE(SC_int_gpio);
    }

    return ret;
}

/*******************************************************
Function:
    Request interrupt.
Input:
    ts: private data.
Output:
    Executive outcomes.
        0: succeed, -1: failed.
*******************************************************/
static s8 SC_request_irq(struct SC_ts_data *ts)
{
    s32 ret = -1;
    const u8 irq_table[] = SC_IRQ_TAB;

    SC_DEBUG_FUNC();
    SC_DEBUG("INT trigger type:%x", ts->int_trigger_type);

    ret  = request_irq(ts->client->irq, 
                       SC_ts_irq_handler,
                       irq_table[ts->int_trigger_type],
                       ts->client->name,
                       ts);
    if (ret)
    {
        SC_ERROR("Request IRQ failed!ERRNO:%d.", ret);
        SC_GPIO_AS_INPUT(SC_int_gpio);
        SC_GPIO_FREE(SC_int_gpio);

        hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        ts->timer.function = SC_ts_timer_handler;
        hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
        return -1;
    }
    else 
    {
        SC_irq_disable(ts);
        ts->use_irq = 1;
        return 0;
    }
}

/*******************************************************
Function:
    Request input device Function.
Input:
    ts:private data.
Output:
    Executive outcomes.
        0: succeed, otherwise: failed.
*******************************************************/
static s8 SC_request_input_dev(struct SC_ts_data *ts)
{
    s8 ret = -1;
  
    SC_DEBUG_FUNC();
  
    ts->input_dev = input_allocate_device();
    if (ts->input_dev == NULL)
    {
        SC_ERROR("Failed to allocate input device.");
        return -ENOMEM;
    }

    ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
    ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
    __set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	#ifdef SC_VIRTRUAL_KEY_SUPPORT
    set_bit(KEY_MENU, ts->input_dev->keybit);
    set_bit(KEY_HOMEPAGE, ts->input_dev->keybit);
    set_bit(KEY_BACK, ts->input_dev->keybit);
	#endif
    #ifdef SC_CTP_SUPPORT_TYPEB_PROTOCOL
	input_mt_init_slots(ts->input_dev, MAX_POINT_NUM,0);
	#endif
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MAX_POINT_NUM - 1, 0, 0);
	#ifdef SC_CTP_PRESSURE
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
    #endif
    ts->input_dev->name = SC_ts_name;
    ts->input_dev->phys = SC_input_phys;
    ts->input_dev->id.bustype = BUS_I2C;
    ts->input_dev->id.vendor = 0xDEAD;
    ts->input_dev->id.product = 0xBEEF;
    ts->input_dev->id.version = 10427;
    
    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        SC_ERROR("Register %s input device failed", ts->input_dev->name);
		input_free_device(ts->input_dev);
        return -ENODEV;
    }
    return 0;
}


/* 
 * Devices Tree support, 
*/
#ifdef SC_CONFIG_OF
/**
 * SC_parse_dt - parse platform infomation form devices tree.
 */
static void SC_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	SC_int_gpio = of_get_named_gpio(np, "SunnyCore,irq-gpio", 0);
	SC_rst_gpio = of_get_named_gpio(np, "SunnyCore,rst-gpio", 0);
	SC_DEBUG("SC_parse_dt:%d,%d .",SC_int_gpio,SC_rst_gpio);
}

#ifdef LDO_POWER_SUPPORT
//static int SC_power_switch(struct i2c_client *client, int on)
int SC_power_switch(struct i2c_client *client, int on)
{
	static struct regulator *vdd_ana;
	#ifndef NO_USE_VCC_i2C
	static struct regulator *vcc_i2c;
	#endif
	int ret;
	if (!vdd_ana) {
		SC_DEBUG("SC_power_switch1.");
		vdd_ana = regulator_get(&client->dev, "vdd_ana");
		if (IS_ERR(vdd_ana)) {
			SC_ERROR("regulator get of vdd_ana failed");
			ret = PTR_ERR(vdd_ana);
			vdd_ana = NULL;
			return ret;
		}
	}
#ifndef NO_USE_VCC_i2C
	if (!vcc_i2c) {
		SC_DEBUG("SC_power_switch2.");
		vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
		if (IS_ERR(vcc_i2c)) {
			SC_ERROR("regulator get of vcc_i2c failed");
			ret = PTR_ERR(vcc_i2c);
			vcc_i2c = NULL;
			goto ERR_GET_VCC;
		}
	}
#endif
	//set 
	if (regulator_count_voltages(vdd_ana) > 0) 
	{
        ret = regulator_set_voltage(vdd_ana,2800000, 2800000);
        if (ret) {
            SC_DEBUG("vdd regulator set_vtg failed ret=%d", ret);
            goto ERR_GET_VCC;
        }
    }
#ifndef NO_USE_VCC_i2C
    if (regulator_count_voltages(vcc_i2c) > 0) {
        ret = regulator_set_voltage(vcc_i2c, 1800000, 1800000);
        if (ret) {
            SC_DEBUG("vcc_i2c regulator set_vtg failed ret=%d", ret);
            goto ERR_GET_VCC;
        }
    }
#endif
	if (on) {
		SC_DEBUG("GTP power on.");
		ret = regulator_enable(vdd_ana);
		SC_DEBUG("GTP power on.ret=%d",ret);
		#ifndef NO_USE_VCC_i2C
		udelay(2);
		ret = regulator_enable(vcc_i2c);
		SC_DEBUG("GTP power on.ret=%d",ret);
		#endif
	} else {
		SC_DEBUG("GTP power off.");
		#ifndef NO_USE_VCC_i2C
		ret = regulator_disable(vcc_i2c);
		udelay(2);
		#endif
		ret = regulator_disable(vdd_ana);
		#ifndef NO_USE_VCC_i2C
		regulator_put(vcc_i2c);
		#endif
		regulator_put(vdd_ana);
	}
	return ret;
	return 0;
	
ERR_GET_VCC:
	regulator_put(vdd_ana);
	return ret;
}
#endif
#endif

/*******************************************************
Function:
    I2c probe.
Input:
    client: i2c device struct.
    id: device id.
Output:
    Executive outcomes. 
        0: succeed.
*******************************************************/
static int SC_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    s32 ret = -1;
    struct SC_ts_data *ts;
    u8 chipID = 0x00;
    
    SC_DEBUG_FUNC();
    //do NOT remove these logs
    SC_INFO("SC Driver Version: %s", SC_DRIVER_VERSION);
   // SC_INFO("SC Driver Built@%s, %s", __TIME__, __DATE__);
    SC_INFO("SC I2C Address: 0x%02x", client->addr);

    SC_i2c_client = client;
    
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
    {
        SC_ERROR("I2C check functionality failed.");
        return -ENODEV;
    }
    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (ts == NULL)
    {
        SC_ERROR("Alloc GFP_KERNEL memory failed.");
        return -ENOMEM;
    }

#ifdef SC_CONFIG_OF	/* device tree support */
    SC_INFO("SC_CONFIG_OF");
    if (client->dev.of_node) {
		SC_parse_dt(&client->dev);
    }
	#ifdef LDO_POWER_SUPPORT
	SC_INFO("SC_CONFIG_OF1");
    ret = SC_power_switch(client, 1);
	if (ret) {
		SC_ERROR("SC power on failed.");
		goto ERR_FREE_BUF;
	}
	#endif
#else			
	SC_rst_gpio = SC_RST_PORT;
	SC_int_gpio = SC_INT_PORT;
#endif

    INIT_WORK(&ts->work, SC_ts_work_func);
    ts->client = client;
    spin_lock_init(&ts->irq_lock);          // 2.6.39 later
    // ts->irq_lock = SPIN_LOCK_UNLOCKED;   // 2.6.39 & before

    mutex_init(&SC_i2c_lock);
	
    i2c_set_clientdata(client, ts);   
    ret = SC_request_io_port(ts);
    if (ret < 0)
    {
        SC_ERROR("SC request IO port failed.");
        goto ERR_RELEASE_POWER;
    }

	SC_ts_reset_wakeup();
    msleep(20);
	//SC_power_switch(ts->client,0);
	SC_ts_set_intmode(0);
    ret = SC_get_chip_id(&chipID);	
    SC_ts_set_intmode(1);
    if (ret < 0)
    {
        SC_ERROR("I2C communication ERROR!");
		
        goto ERR_FREE_GPIO;
    }
    else if(chipID != SC_FLASH_ID)
    {
        SC_ERROR("Please specify the IC model:0x%x!",chipID);
		ret = -1;
		goto ERR_FREE_GPIO;
    }  
    else
    {
        SC_ERROR("I2C communication success:chipID = %x!",chipID);
    }
	ts->abs_x_max = TPD_RES_X;
	ts->abs_y_max = TPD_RES_Y;
	ts->int_trigger_type = 1;

#if defined(SC_AUTO_UPDATE_FARMWARE)
    SC_ts_set_intmode(0);
    ret = SC_auto_update_fw();
	SC_ts_set_intmode(1);
    if (ret < 0)
    {
        SC_ERROR("Create update thread error.");
		goto ERR_FREE_GPIO;
    }
#endif

    ret = SC_request_input_dev(ts);
    if (ret < 0)
    {
        SC_ERROR("SC request input dev failed");
		goto ERR_FREE_GPIO;
    }
    
    ret = SC_request_irq(ts); 
    if (ret < 0)
    {
        SC_INFO("SC works in polling mode.");
    }
    else
    {
        SC_INFO("SC works in interrupt mode.");
    }

    if (ts->use_irq)
    {
        SC_irq_enable(ts);
    }
	
	/* register suspend and resume fucntion*/
	ret = SC_register_powermanger(ts);
	if(ret)
	{
        SC_ERROR("SC_register_powerment register error");
        goto ERR_FREE_HANDLER_AND_UNREGISTER_INPUT_DEVICE;
	}

	#ifdef SC_DEBUGFS_SUPPORT
	ret = sysfs_create_group(&client->dev.kobj, &SC_debug_attr_group);
	if(ret)
    {
        SC_ERROR("SC_debug_attr_group created error!");
		goto ERR_UNREGISTER_POWERMANGER;
    }
	#endif

    return 0;
	
#ifdef SC_DEBUGFS_SUPPORT
ERR_UNREGISTER_POWERMANGER:
    SC_unregister_powermanger(ts);
#endif

ERR_FREE_HANDLER_AND_UNREGISTER_INPUT_DEVICE:
    if(ts->use_irq)
    {
        free_irq(client->irq, ts);
    }
	else
    {
        hrtimer_cancel(&ts->timer);
    }
    input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
	
ERR_FREE_GPIO:
	gpio_free(SC_int_gpio);
    gpio_free(SC_rst_gpio);

ERR_RELEASE_POWER:
	#ifdef LDO_POWER_SUPPORT
	SC_power_switch(client,0);
	#endif

ERR_FREE_BUF:
	i2c_set_clientdata(client,NULL);
    kfree(ts);
	
    return ret;
}


/*******************************************************
Function:
    Goodix touchscreen driver release function.
Input:
    client: i2c device struct.
Output:
    Executive outcomes. 0---succeed.
*******************************************************/
static int SC_ts_remove(struct i2c_client *client)
{
    struct SC_ts_data *ts = i2c_get_clientdata(client);
    
    SC_DEBUG_FUNC();
    #ifdef SC_DEBUGFS_SUPPORT
	sysfs_remove_group(&client->dev.kobj,&SC_debug_attr_group);
    #endif
	SC_unregister_powermanger(ts);

    if (ts) 
    {
        if (ts->use_irq)
        {
            SC_GPIO_AS_INPUT(SC_int_gpio);
            SC_GPIO_FREE(SC_int_gpio);
            free_irq(client->irq, ts);
        }
        else
        {
            hrtimer_cancel(&ts->timer);
        }
    }   
    
    SC_INFO("SC driver removing...");
    i2c_set_clientdata(client, NULL);
    input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
	SC_GPIO_FREE(SC_rst_gpio);
    kfree(ts);

    return 0;
}


/*******************************************************
Function:
    Early suspend function.
Input:
    h: early_suspend struct.
Output:
    None.
*******************************************************/
static void SC_ts_suspend(struct SC_ts_data *ts)
{
    s8 ret = -1;    
    
    SC_DEBUG_FUNC();
    if (ts->enter_update) {
    	return;
    }
    SC_INFO("System suspend.");
	
	//SC_power_switch(ts->client,0);
	
    ts->SC_is_suspend = 1;

    if (ts->use_irq)
    {
        SC_irq_disable(ts);
    }
    else
    {
        hrtimer_cancel(&ts->timer);
    }
    ret = SC_enter_sleep(ts);

    if (ret < 0)
    {
        SC_ERROR("SC early suspend failed.");
    }
    msleep(58);   
	
}

/*******************************************************
Function:
    Late resume function.
Input:
    h: early_suspend struct.
Output:
    None.
*******************************************************/
static void SC_ts_resume(struct SC_ts_data *ts)
{
    s8 ret = -1; 
    SC_DEBUG_FUNC();
    if (ts->enter_update) {
    	return;
    }
	
	//SC_power_switch(ts->client,1);
	
    SC_INFO("System resume.");
    
    ret = SC_wakeup_sleep(ts);

    if (ret < 0)
    {
        SC_ERROR("SC later resume failed.");
    }

    if (ts->use_irq)
    {
        SC_irq_enable(ts);
    }
    else
    {
        hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
    }

    ts->SC_is_suspend = 0;
}


#if   defined(CONFIG_FB)	
/* frame buffer notifier block control the suspend/resume procedure */
static int SC_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct SC_ts_data *ts = container_of(noti, struct SC_ts_data, notifier);
	int *blank;
	
	if (ev_data && ev_data->data && event == FB_EVENT_BLANK && ts) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			SC_DEBUG("Resume by fb notifier.");
			SC_ts_resume(ts);
				
		}
		else if (*blank == FB_BLANK_POWERDOWN) {
			SC_DEBUG("Suspend by fb notifier.");
			SC_ts_suspend(ts);
		}
	}

	return 0;
}
#elif defined(CONFIG_PM)
/* bus control the suspend/resume procedure */
static int SC_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SC_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		SC_DEBUG("Suspend by i2c pm.");
		SC_ts_suspend(ts);
	}

	return 0;
}
static int SC_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SC_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		SC_DEBUG("Resume by i2c pm.");
		SC_ts_resume(ts);
	}

	return 0;
}

static struct dev_pm_ops SC_pm_ops = {
	.suspend = SC_pm_suspend,
	.resume  = SC_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void SC_ts_early_suspend(struct early_suspend *h)
{
	struct SC_ts_data *ts = container_of(h, struct SC_ts_data, early_suspend);

	if (ts) {
		SC_DEBUG("Suspend by earlysuspend module.");
		SC_ts_suspend(ts);
	}
}
static void SC_ts_late_resume(struct early_suspend *h)
{
	struct SC_ts_data *ts = container_of(h, struct SC_ts_data, early_suspend);

	if (ts) {
		SC_DEBUG("Resume by earlysuspend module.");
		SC_ts_resume(ts);
	}	
}
#endif

static int SC_register_powermanger(struct SC_ts_data *ts)
{
    int ret = 0;
#if   defined(CONFIG_FB)
	ts->notifier.notifier_call = SC_fb_notifier_callback;
	ret = fb_register_client(&ts->notifier);
	if(ret)
    {
        SC_ERROR("register fb_register_client error");
	//LIU
        //ERR_PTR(ret);
    }
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = SC_ts_early_suspend;
	ts->early_suspend.resume = SC_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif	

	return ret;
}

static int SC_unregister_powermanger(struct SC_ts_data *ts)
{
#if   defined(CONFIG_FB)
		fb_unregister_client(&ts->notifier);
		
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts->early_suspend);
#endif
	return 0;
}

/* end */


#ifdef SC_CONFIG_OF
static const struct of_device_id SC_match_table[] = {
		{.compatible = "SC,SunnyCore_ts",},
		{ },
};
#endif

static const struct i2c_device_id SC_ts_id[] = {
    { SC_I2C_NAME, 0 },
    { }
};

static struct i2c_driver SC_ts_driver = {
    .probe      = SC_ts_probe,
    .remove     = SC_ts_remove,
    .id_table   = SC_ts_id,
    .driver = {
        .name     = SC_I2C_NAME,
        .owner    = THIS_MODULE,
#ifdef SC_CONFIG_OF
        .of_match_table = SC_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		  = &SC_pm_ops,
#endif
    },
};

/*******************************************************    
Function:
    Driver Install function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static int __init SC_ts_init(void)
{
    s32 ret;

    SC_DEBUG_FUNC();   
    SC_INFO("SC driver installing...");
    SC_wq = create_singlethread_workqueue("SC_wq");
    if (!SC_wq)
    {
        SC_ERROR("Creat workqueue failed.");
        return -ENOMEM;
    }
    ret = i2c_add_driver(&SC_ts_driver);
    return ret; 
}

/*******************************************************    
Function:
    Driver uninstall function.
Input:
    None.
Output:
    Executive Outcomes. 0---succeed.
********************************************************/
static void __exit SC_ts_exit(void)
{
    SC_DEBUG_FUNC();
    SC_INFO("SC driver exited.");
    i2c_del_driver(&SC_ts_driver);
    if (SC_wq)
    {
        destroy_workqueue(SC_wq);
    }
}

late_initcall(SC_ts_init);
module_exit(SC_ts_exit);

MODULE_DESCRIPTION("SC Series Driver");
MODULE_LICENSE("GPL");
