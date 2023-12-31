/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

//prize add by wangfei for lcd hardware info 20210726 start
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//prize add by wangfei for lcd hardware info 20210726 end

/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;


/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,i2c_lcd_bias",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	pr_err("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_err("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/***********************************/

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control start */
	struct gpio_desc *ldo18_en_gpio;
	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control end */
	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	
	pr_err("gezi----------%s----%d\n",__func__,__LINE__);
	
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x87,0x20,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xFF,0x87,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x2A,0x00,0x00,0x04,0x37);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x2B,0x00,0x00,0x09,0x9B);
	lcm_dcs_write_seq_static(ctx,0x00,0xA3);
	lcm_dcs_write_seq_static(ctx,0xB3,0x09,0x9C,0x00,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0x60);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x80,0x00,0x0F,0x00,0x11);
	lcm_dcs_write_seq_static(ctx,0x00,0x70);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0xC8,0x00,0xC8,0x0D,0x02,0x2B);
	lcm_dcs_write_seq_static(ctx,0x00,0x79);
	lcm_dcs_write_seq_static(ctx,0xC0,0x12,0x00,0xD6);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x4F,0x00,0x0F,0x00,0x11);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x4F,0x00,0x0F,0x00,0x11);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x55,0x00,0x0F,0x00,0x11);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x93,0x00,0x0F,0x11);
	lcm_dcs_write_seq_static(ctx,0x00,0xA3);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00,0x46,0x00,0x20,0x00,0x02);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCE,0x01,0x81,0xFF,0xFF,0x00,0xBC,0x00,0xCC);
	lcm_dcs_write_seq_static(ctx,0x00,0x8C);
	lcm_dcs_write_seq_static(ctx,0xCE,0x01,0x2C,0x01,0x2C);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x86,0x0B,0x46,0x00,0x86,0x80,0xFF,0xFF,0x00,0x04,0x00,0x10,0x0D);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCE,0x22,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xE1);
	lcm_dcs_write_seq_static(ctx,0xCE,0x0A,0x02,0x2B,0x02,0x2B);
	lcm_dcs_write_seq_static(ctx,0x00,0xF1);
	lcm_dcs_write_seq_static(ctx,0xCE,0x1B,0x1B);
	lcm_dcs_write_seq_static(ctx,0x00,0xF4);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0xD5,0x00,0xD5);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCF,0x00,0x00,0xA0,0xA4);
	lcm_dcs_write_seq_static(ctx,0x00,0xB5);
	lcm_dcs_write_seq_static(ctx,0xCF,0x05,0x05,0x1C,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xCF,0x09,0x09,0x97,0x9B);
	lcm_dcs_write_seq_static(ctx,0x00,0xC5);
	lcm_dcs_write_seq_static(ctx,0xCF,0x09,0x09,0x9D,0xA1);
	lcm_dcs_write_seq_static(ctx,0x00,0xD1);
	lcm_dcs_write_seq_static(ctx,0xC1,0x0A,0xF3,0x0F,0x36,0x19,0xF6,0x08,0x32,0x0B,0x6C,0x13,0x85);
	lcm_dcs_write_seq_static(ctx,0x00,0xE1);
	lcm_dcs_write_seq_static(ctx,0xC1,0x0F,0x36);
	lcm_dcs_write_seq_static(ctx,0x00,0xE4);
	lcm_dcs_write_seq_static(ctx,0xCF,0x09,0xBF,0x09,0xBE,0x09,0xBE,0x09,0xBE,0x09,0xBE,0x09,0xBE);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xC1,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0xCC);
	lcm_dcs_write_seq_static(ctx,0xC1,0x18);
	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x86);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x02,0x00,0x00,0x0D,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0xB3);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x02,0x00,0x00,0x0D,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0x96);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x02,0x00,0x00,0x0D,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0xE8);
	lcm_dcs_write_seq_static(ctx,0xC0,0x40);
	lcm_dcs_write_seq_static(ctx,0x00,0x77);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x26);
	lcm_dcs_write_seq_static(ctx,0x00,0xD1);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x0A,0x01,0x01,0x00,0xEB,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0xE8);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x26);
	lcm_dcs_write_seq_static(ctx,0x00,0xA1);
	lcm_dcs_write_seq_static(ctx,0xF3,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0xA3);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x02,0x00,0x00,0x0A,0x07);
	lcm_dcs_write_seq_static(ctx,0x00,0xA6);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x00,0x00,0x01,0x0F,0x06);
	lcm_dcs_write_seq_static(ctx,0x00,0x66);
	lcm_dcs_write_seq_static(ctx,0xC0,0x00,0x02,0x00,0x00,0x1E,0x08);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x83);
	lcm_dcs_write_seq_static(ctx,0xB0,0x63);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xB3,0x22);
	lcm_dcs_write_seq_static(ctx,0x00,0xF0);
	lcm_dcs_write_seq_static(ctx,0xC1,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xF5);
	lcm_dcs_write_seq_static(ctx,0xCF,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC2,0x84,0x01,0x48,0x0B,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xC2,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x81,0x04,0x00,0x02,0x8E,0x80,0x01,0x00,0x02,0x8E,0x82,0x02,0x00,0x02,0x8E);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x83,0x03,0x00,0x02,0x8E,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x33,0x33,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x8C);
	lcm_dcs_write_seq_static(ctx,0xC3,0x01,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xD0);
	lcm_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x05,0x00,0x00,0x05,0x00,0x00,0x0E,0xCE,0x01,0xC5,0x00,0x00,0x00,0xC0,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x30,0x00,0x0C,0x3C,0x00,0x30,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x10,0x42,0x94,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x10,0x42,0x94,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xD5);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCC,0x23,0x23,0x23,0x16,0x17,0x18,0x23,0x23,0x23,0x16,0x17,0x18,0x1F,0x02,0x07,0x08);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCC,0x06,0x09,0x1D,0x1E,0x23,0x23,0x21,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x23,0x23,0x23,0x16,0x17,0x18,0x23,0x23,0x23,0x16,0x17,0x18,0x1F,0x02,0x06,0x09);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x07,0x08,0x1E,0x1D,0x23,0x23,0x21,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCD,0x23,0x23,0x23,0x16,0x17,0x18,0x23,0x23,0x23,0x16,0x17,0x18,0x1F,0x02,0x07,0x08);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCD,0x06,0x09,0x1D,0x1E,0x23,0x23,0x21,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x23,0x23,0x23,0x16,0x17,0x18,0x23,0x23,0x23,0x16,0x17,0x18,0x1F,0x02,0x06,0x09);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x07,0x08,0x1E,0x1D,0x23,0x23,0x21,0x20);
	lcm_dcs_write_seq_static(ctx,0x00,0x93);
	lcm_dcs_write_seq_static(ctx,0xC5,0x37);
	lcm_dcs_write_seq_static(ctx,0x00,0x97);
	lcm_dcs_write_seq_static(ctx,0xC5,0x37);
	lcm_dcs_write_seq_static(ctx,0x00,0x9A);
	lcm_dcs_write_seq_static(ctx,0xC5,0x2D);
	lcm_dcs_write_seq_static(ctx,0x00,0x9C);
	lcm_dcs_write_seq_static(ctx,0xC5,0x2D);
	lcm_dcs_write_seq_static(ctx,0x00,0xB6);
	lcm_dcs_write_seq_static(ctx,0xC5,0x19,0x19,0x0A,0x0A,0x0F,0x0F,0x0A,0x0A);
	lcm_dcs_write_seq_static(ctx,0x00,0x88);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xA7,0x10);
	lcm_dcs_write_seq_static(ctx,0x00,0xCA);
	lcm_dcs_write_seq_static(ctx,0xC0,0x90);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xC3,0xC9);
	lcm_dcs_write_seq_static(ctx,0x00,0x88);
	lcm_dcs_write_seq_static(ctx,0xC1,0x8F);
	lcm_dcs_write_seq_static(ctx,0x00,0x9C);
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x9E);
	lcm_dcs_write_seq_static(ctx,0xF5,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x86);
	lcm_dcs_write_seq_static(ctx,0xF5,0x4B);
	lcm_dcs_write_seq_static(ctx,0x00,0x96);
	lcm_dcs_write_seq_static(ctx,0xF5,0x0C);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xD8,0x2B,0x2B);
	lcm_dcs_write_seq_static(ctx,0x00,0x06);
	lcm_dcs_write_seq_static(ctx,0xD9,0x23,0x23,0x23);
	lcm_dcs_write_seq_static(ctx,0x00,0x9B);
	lcm_dcs_write_seq_static(ctx,0xC4,0xFF);
	lcm_dcs_write_seq_static(ctx,0x00,0x94);
	lcm_dcs_write_seq_static(ctx,0xE9,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0x30);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0x60);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0xF0);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x03,0x11,0x3F,0x1E,0x27,0x2E,0x3A,0xD8,0x43,0x4B,0x51,0x57,0x9E,0x5C);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE2,0x65,0x6D,0x75,0xAD,0x7C,0x84,0x8C,0x95,0xD9,0x9F,0xA5,0xAC,0xB3,0xE2,0xBC,0xC6,0xD3,0xDB,0x9B,0xE5,0xF2,0xFB,0xFF,0x93);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xA7,0x03);
	lcm_dcs_write_seq_static(ctx,0x00,0x82);
	lcm_dcs_write_seq_static(ctx,0xA7,0x22);
	lcm_dcs_write_seq_static(ctx,0x00,0x8D);
	lcm_dcs_write_seq_static(ctx,0xA7,0x02);
	lcm_dcs_write_seq_static(ctx,0x00,0x99);
	lcm_dcs_write_seq_static(ctx,0xCF,0x50);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xC5,0xD0,0x4A,0x31,0xD0,0x4A,0x0C);
	lcm_dcs_write_seq_static(ctx,0x00,0x92);
	lcm_dcs_write_seq_static(ctx,0xC5,0x00);
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC5,0x40);
	lcm_dcs_write_seq_static(ctx,0x00,0x98);
	lcm_dcs_write_seq_static(ctx,0xC5,0x27);
	lcm_dcs_write_seq_static(ctx,0x00,0x94);
	lcm_dcs_write_seq_static(ctx,0xC5,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xA4,0xC8);
	lcm_dcs_write_seq_static(ctx,0x00,0x8D);
	lcm_dcs_write_seq_static(ctx,0xC5,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0x8A);
	lcm_dcs_write_seq_static(ctx,0xC5,0x04);
	lcm_dcs_write_seq_static(ctx,0x00,0xF0);
	lcm_dcs_write_seq_static(ctx,0xCF,0x01,0x78);
	lcm_dcs_write_seq_static(ctx,0x00,0xA4);
	lcm_dcs_write_seq_static(ctx,0xD7,0x9F);
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xC5,0x85);
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCF,0x00,0x00,0xC0,0xC4);
	lcm_dcs_write_seq_static(ctx,0x00,0xB5);
	lcm_dcs_write_seq_static(ctx,0xCF,0x05,0x05,0x70,0x74);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x1C,0x02);

	lcm_dcs_write_seq_static(ctx, 0x11,0x00);
	msleep(180);
	lcm_dcs_write_seq_static(ctx, 0x29,0x00);
	msleep(100);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#else
	/*ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);*/

	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(1000);
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control start */
	udelay(10000);
	ctx->ldo18_en_gpio = devm_gpiod_get_index(ctx->dev,
		"pm-enable", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ldo18_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get ldo18_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->ldo18_en_gpio));
		return PTR_ERR(ctx->ldo18_en_gpio);
	}
	dev_info(ctx->dev, "%s: gpio ldo18_en_gpio = %d\n", __func__, desc_to_gpio(ctx->ldo18_en_gpio));
	gpiod_set_value(ctx->ldo18_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->ldo18_en_gpio);
	
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	dev_info(ctx->dev, "%s: gpio reset_gpio = %d\n", __func__, desc_to_gpio(ctx->reset_gpio));
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control end */

	pr_err("gezi------exit----%s-----%d\n",__func__,__LINE__);
#endif
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
	
	if (ctx->prepared)
		return 0;

	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control start */
	ctx->ldo18_en_gpio = devm_gpiod_get_index(ctx->dev,
		"pm-enable", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ldo18_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get ldo18_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->ldo18_en_gpio));
		return PTR_ERR(ctx->ldo18_en_gpio);
	}
	dev_info(ctx->dev, "%s: gpio ldo18_en_gpio = %d\n", __func__, desc_to_gpio(ctx->ldo18_en_gpio));
	gpiod_set_value(ctx->ldo18_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->ldo18_en_gpio);
	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control end */


#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#else
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	
	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#endif

	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control start */
	udelay(10000);
	/* Prize HanJiuping add 20210629 for ext LCM 1.8V LDO control end */
	
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#endif
	pr_err("gezi----exit------%s-----%d\n",__func__,__LINE__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("gezi----exit--11111----%s-----%d\n",__func__,__LINE__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
	pr_err("gezi----exit---if enter---%s-----%d\n",__func__,__LINE__);
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	pr_err("gezi----exit---22222---%s-----%d\n",__func__,__LINE__);
	ctx->enabled = true;

	return 0;
}

#define HFP (10)
#define HSA (10)
#define HBP (25)
#define VFP (20)
#define VSA (2)
#define VBP (10)
#define VAC (2460)
#define HAC (1080)
static u32 fake_heigh = 2400;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 538000,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x40, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 530,
	.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
//prize add by wangfei for lcd size 6.78 20210717 start
	.physical_width_um = 68500,
	.physical_height_um = 157900,
//prize add by wangfei for lcd size 6.78 20210717 end
	.wait_sof_before_dec_vfp = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);
//prize add by wangfei for lcd size 6.78 20210717 start
	panel->connector->display_info.width_mm = 69;
	panel->connector->display_info.height_mm = 158;
//prize add by wangfei for lcd size 6.78 20210717 end
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
	
	pr_err("%s------need_fake_resolution = %d------%d\n", __func__,need_fake_resolution,__LINE__);
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
	pr_err("%s------------%d\n", __func__,__LINE__);
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
        
    strcpy(current_lcm_info.chip,"ft8720");
    strcpy(current_lcm_info.vendor,"focaltech");
    sprintf(current_lcm_info.id,"0x%02x",0x80);
    strcpy(current_lcm_info.more,"LCM");
        
#endif

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}
/* Prize hanwei modified for lcm information  20210713 start */
static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "focaltech,ft8720,vdo", },
	{ }
};
/* Prize hanwei modified for lcm information 20210713 end */
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-truly-ft8720-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Tai-Hua Tseng <tai-hua.tseng@mediatek.com>");
MODULE_DESCRIPTION("truly ft8720 VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
