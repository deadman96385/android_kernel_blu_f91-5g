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

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "prize_lcd_bias.h"

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

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
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
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
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(50);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(100);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

lcm_dcs_write_seq_static(ctx,0xFF,0x98, 0x82, 0x01);
lcm_dcs_write_seq_static(ctx,0x00,0x42);
lcm_dcs_write_seq_static(ctx,0x01,0x33);
lcm_dcs_write_seq_static(ctx,0x02,0x01);
lcm_dcs_write_seq_static(ctx,0x03,0x00);
lcm_dcs_write_seq_static(ctx,0x04,0x02);
lcm_dcs_write_seq_static(ctx,0x05,0x31);
lcm_dcs_write_seq_static(ctx,0x07,0x00);
lcm_dcs_write_seq_static(ctx,0x08,0x80);
lcm_dcs_write_seq_static(ctx,0x09,0x81);
lcm_dcs_write_seq_static(ctx,0x0a,0x71);
lcm_dcs_write_seq_static(ctx,0x0b,0x00);
lcm_dcs_write_seq_static(ctx,0x0c,0x01);
lcm_dcs_write_seq_static(ctx,0x0d,0x01);
lcm_dcs_write_seq_static(ctx,0x0e,0x00);
lcm_dcs_write_seq_static(ctx,0x0f,0x00);
lcm_dcs_write_seq_static(ctx,0x28,0x88);
lcm_dcs_write_seq_static(ctx,0x29,0x88);
lcm_dcs_write_seq_static(ctx,0x2a,0x00);
lcm_dcs_write_seq_static(ctx,0x2b,0x00);
lcm_dcs_write_seq_static(ctx,0x31,0x07);
lcm_dcs_write_seq_static(ctx,0x32,0x0c);
lcm_dcs_write_seq_static(ctx,0x33,0x22);
lcm_dcs_write_seq_static(ctx,0x34,0x23);
lcm_dcs_write_seq_static(ctx,0x35,0x07);
lcm_dcs_write_seq_static(ctx,0x36,0x08);
lcm_dcs_write_seq_static(ctx,0x37,0x16);
lcm_dcs_write_seq_static(ctx,0x38,0x14);
lcm_dcs_write_seq_static(ctx,0x39,0x12);
lcm_dcs_write_seq_static(ctx,0x3a,0x10);
lcm_dcs_write_seq_static(ctx,0x3b,0x21);
lcm_dcs_write_seq_static(ctx,0x3c,0x07);
lcm_dcs_write_seq_static(ctx,0x3d,0x07);
lcm_dcs_write_seq_static(ctx,0x3e,0x07);
lcm_dcs_write_seq_static(ctx,0x3f,0x07);
lcm_dcs_write_seq_static(ctx,0x40,0x07);
lcm_dcs_write_seq_static(ctx,0x41,0x07);
lcm_dcs_write_seq_static(ctx,0x42,0x07);
lcm_dcs_write_seq_static(ctx,0x43,0x07);
lcm_dcs_write_seq_static(ctx,0x44,0x07);
lcm_dcs_write_seq_static(ctx,0x45,0x07);
lcm_dcs_write_seq_static(ctx,0x46,0x07);
lcm_dcs_write_seq_static(ctx,0x47,0x07);
lcm_dcs_write_seq_static(ctx,0x48,0x0d);
lcm_dcs_write_seq_static(ctx,0x49,0x22);
lcm_dcs_write_seq_static(ctx,0x4a,0x23);
lcm_dcs_write_seq_static(ctx,0x4b,0x07);
lcm_dcs_write_seq_static(ctx,0x4c,0x09);
lcm_dcs_write_seq_static(ctx,0x4d,0x17);
lcm_dcs_write_seq_static(ctx,0x4e,0x15);
lcm_dcs_write_seq_static(ctx,0x4f,0x13);
lcm_dcs_write_seq_static(ctx,0x50,0x11);
lcm_dcs_write_seq_static(ctx,0x51,0x21);
lcm_dcs_write_seq_static(ctx,0x52,0x07);
lcm_dcs_write_seq_static(ctx,0x53,0x07);
lcm_dcs_write_seq_static(ctx,0x54,0x07);
lcm_dcs_write_seq_static(ctx,0x55,0x07);
lcm_dcs_write_seq_static(ctx,0x56,0x07);
lcm_dcs_write_seq_static(ctx,0x57,0x07);
lcm_dcs_write_seq_static(ctx,0x58,0x07);
lcm_dcs_write_seq_static(ctx,0x59,0x07);
lcm_dcs_write_seq_static(ctx,0x5a,0x07);
lcm_dcs_write_seq_static(ctx,0x5b,0x07);
lcm_dcs_write_seq_static(ctx,0x5c,0x07);
lcm_dcs_write_seq_static(ctx,0x61,0x07);
lcm_dcs_write_seq_static(ctx,0x62,0x0d);
lcm_dcs_write_seq_static(ctx,0x63,0x22);
lcm_dcs_write_seq_static(ctx,0x64,0x23);
lcm_dcs_write_seq_static(ctx,0x65,0x07);
lcm_dcs_write_seq_static(ctx,0x66,0x09);
lcm_dcs_write_seq_static(ctx,0x67,0x11);
lcm_dcs_write_seq_static(ctx,0x68,0x13);
lcm_dcs_write_seq_static(ctx,0x69,0x15);
lcm_dcs_write_seq_static(ctx,0x6a,0x17);
lcm_dcs_write_seq_static(ctx,0x6b,0x21);
lcm_dcs_write_seq_static(ctx,0x6c,0x07);
lcm_dcs_write_seq_static(ctx,0x6d,0x07);
lcm_dcs_write_seq_static(ctx,0x6e,0x07);
lcm_dcs_write_seq_static(ctx,0x6f,0x07);
lcm_dcs_write_seq_static(ctx,0x70,0x07);
lcm_dcs_write_seq_static(ctx,0x71,0x07);
lcm_dcs_write_seq_static(ctx,0x72,0x07);
lcm_dcs_write_seq_static(ctx,0x73,0x07);
lcm_dcs_write_seq_static(ctx,0x74,0x07);
lcm_dcs_write_seq_static(ctx,0x75,0x07);
lcm_dcs_write_seq_static(ctx,0x76,0x07);
lcm_dcs_write_seq_static(ctx,0x77,0x07);
lcm_dcs_write_seq_static(ctx,0x78,0x0c);
lcm_dcs_write_seq_static(ctx,0x79,0x22);
lcm_dcs_write_seq_static(ctx,0x7a,0x23);
lcm_dcs_write_seq_static(ctx,0x7b,0x07);
lcm_dcs_write_seq_static(ctx,0x7c,0x08);
lcm_dcs_write_seq_static(ctx,0x7d,0x10);
lcm_dcs_write_seq_static(ctx,0x7e,0x12);
lcm_dcs_write_seq_static(ctx,0x7f,0x14);
lcm_dcs_write_seq_static(ctx,0x80,0x16);
lcm_dcs_write_seq_static(ctx,0x81,0x21);
lcm_dcs_write_seq_static(ctx,0x82,0x07);
lcm_dcs_write_seq_static(ctx,0x83,0x07);
lcm_dcs_write_seq_static(ctx,0x84,0x07);
lcm_dcs_write_seq_static(ctx,0x85,0x07);
lcm_dcs_write_seq_static(ctx,0x86,0x07);
lcm_dcs_write_seq_static(ctx,0x87,0x07);
lcm_dcs_write_seq_static(ctx,0x88,0x07);
lcm_dcs_write_seq_static(ctx,0x89,0x07);
lcm_dcs_write_seq_static(ctx,0x8a,0x07);
lcm_dcs_write_seq_static(ctx,0x8b,0x07);
lcm_dcs_write_seq_static(ctx,0x8c,0x07);
lcm_dcs_write_seq_static(ctx,0xb0,0x33);
lcm_dcs_write_seq_static(ctx,0xba,0x06);
lcm_dcs_write_seq_static(ctx,0xc0,0x07);
lcm_dcs_write_seq_static(ctx,0xc1,0x00);
lcm_dcs_write_seq_static(ctx,0xc2,0x00);
lcm_dcs_write_seq_static(ctx,0xc3,0x00);
lcm_dcs_write_seq_static(ctx,0xc4,0x00);
lcm_dcs_write_seq_static(ctx,0xca,0x44);
lcm_dcs_write_seq_static(ctx,0xd0,0x01);
lcm_dcs_write_seq_static(ctx,0xd1,0x22);
lcm_dcs_write_seq_static(ctx,0xd3,0x40);
lcm_dcs_write_seq_static(ctx,0xd5,0x51);
lcm_dcs_write_seq_static(ctx,0xd6,0x20);
lcm_dcs_write_seq_static(ctx,0xd7,0x01);
lcm_dcs_write_seq_static(ctx,0xd8,0x00);
lcm_dcs_write_seq_static(ctx,0xdc,0xc2);
lcm_dcs_write_seq_static(ctx,0xdd,0x10);
lcm_dcs_write_seq_static(ctx,0xdf,0xb6);
lcm_dcs_write_seq_static(ctx,0xe0,0x3E);
lcm_dcs_write_seq_static(ctx,0xe2,0x47);
lcm_dcs_write_seq_static(ctx,0xe7,0x54);
lcm_dcs_write_seq_static(ctx,0xe6,0x22);
lcm_dcs_write_seq_static(ctx,0xee,0x15);
lcm_dcs_write_seq_static(ctx,0xf1,0x00);
lcm_dcs_write_seq_static(ctx,0xf2,0x00);
lcm_dcs_write_seq_static(ctx,0xfa,0xdf);
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x02);
lcm_dcs_write_seq_static(ctx,0xF1,0x1C );
lcm_dcs_write_seq_static(ctx,0x40,0x4B );
lcm_dcs_write_seq_static(ctx,0x4B,0x5A );
lcm_dcs_write_seq_static(ctx,0x50,0xCA );
lcm_dcs_write_seq_static(ctx,0x51,0x10 );
lcm_dcs_write_seq_static(ctx,0x06,0x8D );
lcm_dcs_write_seq_static(ctx,0x0B,0xA0 );
lcm_dcs_write_seq_static(ctx,0x0C,0x80 );
lcm_dcs_write_seq_static(ctx,0x0D,0x1A );
lcm_dcs_write_seq_static(ctx,0x0E,0x04 );
lcm_dcs_write_seq_static(ctx,0x4D,0xCE );
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x05 );
lcm_dcs_write_seq_static(ctx,0xA8,0x62 );
lcm_dcs_write_seq_static(ctx,0x03,0x00 );
lcm_dcs_write_seq_static(ctx,0x04,0xA7 );
lcm_dcs_write_seq_static(ctx,0x63,0x73 );
lcm_dcs_write_seq_static(ctx,0x64,0x73 );
lcm_dcs_write_seq_static(ctx,0x68,0x65 );
lcm_dcs_write_seq_static(ctx,0x69,0x6B );
lcm_dcs_write_seq_static(ctx,0x6A,0xA1 );
lcm_dcs_write_seq_static(ctx,0x6B,0x93 );
lcm_dcs_write_seq_static(ctx,0x00,0x01 );
lcm_dcs_write_seq_static(ctx,0x46,0x00 );
lcm_dcs_write_seq_static(ctx,0x45,0x01 );
lcm_dcs_write_seq_static(ctx,0x17,0x50 );
lcm_dcs_write_seq_static(ctx,0x18,0x01 );
lcm_dcs_write_seq_static(ctx,0x85,0x37 );
lcm_dcs_write_seq_static(ctx,0x86,0x0F );
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x06);
lcm_dcs_write_seq_static(ctx,0xD9,0x1F );
lcm_dcs_write_seq_static(ctx,0xC0,0x40 );
lcm_dcs_write_seq_static(ctx,0xC1,0x16 );
lcm_dcs_write_seq_static(ctx,0x07,0xF7 );
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x08 );
lcm_dcs_write_seq_static(ctx,0xE0,0x40,0x24,0x97,0xD1,0x10,0x55,0x40,0x64,0x8D,0xAD,0xA9,0xDE,0x03,0x24,0x43,0xAA,0x63,0x8C,0xA8,0xCC,0xFE,0xEC,0x18,0x50,0x7D,0x03,0xEC);
lcm_dcs_write_seq_static(ctx,0xE1,0x40,0x24,0x97,0xD1,0x10,0x55,0x40,0x64,0x8D,0xAD,0xA9,0xDE,0x03,0x24,0x43,0xAA,0x63,0x8C,0xA8,0xCC,0xFE,0xEC,0x18,0x50,0x7D,0x03,0xEC);
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x0B );
lcm_dcs_write_seq_static(ctx,0x9A,0x44);
lcm_dcs_write_seq_static(ctx,0x9B,0x88);
lcm_dcs_write_seq_static(ctx,0x9C,0x03);
lcm_dcs_write_seq_static(ctx,0x9D,0x03);
lcm_dcs_write_seq_static(ctx,0x9E,0x71);
lcm_dcs_write_seq_static(ctx,0x9F,0x71);
lcm_dcs_write_seq_static(ctx,0xAB,0xE0);
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x0E );
lcm_dcs_write_seq_static(ctx,0x02,0x09 );
lcm_dcs_write_seq_static(ctx,0x11,0x50 );
lcm_dcs_write_seq_static(ctx,0x13,0x10 );
lcm_dcs_write_seq_static(ctx,0x00,0xA0 );
lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x00 );
lcm_dcs_write_seq_static(ctx,0x35,0x00 );
lcm_dcs_write_seq_static(ctx,0x51,0x7F );
lcm_dcs_write_seq_static(ctx,0x53,0x2C );

lcm_dcs_write_seq_static(ctx,0x35,0x00);



//lcm_dcs_write_seq_static(ctx,0x1C,0x10); 720*1600

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	msleep(30);
	lcm_dcs_write_seq_static(ctx, 0x35,0x00);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
		
	if (!ctx->enabled)
		return 0;
	pr_info("%s\n", __func__);
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	pr_info("%s_end\n", __func__);
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;
//lcm huangjiwu  for tp   20200821 --begin
#if !defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
//lcm huangjiwu  for tp   20200821 --end
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
//lcm huangjiwu  for tp   20200821 --begin
	pr_info("%s_reset_gpio\n", __func__);
#endif
//lcm huangjiwu  for tp   20200821 --end
#if defined(CONFIG_PRIZE_LCD_BIAS)
//lcm huangjiwu  for tp   20200821 --begin
#if !defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
	display_bias_disable();
#endif
//lcm huangjiwu  for tp   20200821 --end
#else
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

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_PRIZE_LCD_BIAS)
	display_bias_enable();
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

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
#endif

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s\n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
	pr_info("%s_end\n", __func__);
	ctx->enabled = true;

	return 0;
}

#define HFP (30)
#define HSA (20)
#define HBP (20)
#define VFP (400)
#define VSA (20)
#define VBP (20)
#define VAC (1600)//1920  2400    2040
#define HAC (720)//1080  1080    790
static const struct drm_display_mode default_mode = {
	.clock = 96696,  // 163943  135930  //htotal*vtotal*vrefresh/1000     htotal = HAC + HFP + HSA + HBP=1150   vtotal = VAC + VFP + VSA + VBP=1970
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
//lcm huangjiwu  for tp   20200821 --begin
#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
static int panel_poweroff_ext(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
		ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	pr_info("%s_reset_gpio\n", __func__);
	display_bias_disable();
	return 0;
}
#endif
//lcm huangjiwu  for tp   20200821 --end
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
	unsigned char id[3] = {0x00, 0x00, 0x00};
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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 290,   //420  509
	.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},

};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	//lcm huangjiwu  for tp   20200821 --begin
	#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
	.poweroff_ext = panel_poweroff_ext,
	#endif
	//lcm huangjiwu  for tp   20200821 --end
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

	panel->connector->display_info.width_mm = 70;
	panel->connector->display_info.height_mm = 155;//sqrt((size*25.4)^2/(1600^2+720^2))*1600

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

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
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS |MIPI_DSI_MODE_VIDEO;

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

	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "panda,ili9882h,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-panda-ili9882h-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("truly td4330 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
