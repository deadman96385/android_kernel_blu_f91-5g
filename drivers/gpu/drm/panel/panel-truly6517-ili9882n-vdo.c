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

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
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

static struct notifier_block fb_nb;

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
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
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);//ili9882n at least 10ms
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


#if 1      //OPPO 6.517项目20201114。  4H代码
					// GIP Setting
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x01);
lcm_dcs_write_seq_static(ctx, 0x00,0x48);    //4E STVA
lcm_dcs_write_seq_static(ctx, 0x01,0x33);      //17   STV 4H
lcm_dcs_write_seq_static(ctx, 0x02,0x00);      //STV rise adjust
lcm_dcs_write_seq_static(ctx, 0x03,0x00);      //STV fall adjust  20190129
lcm_dcs_write_seq_static(ctx, 0x04,0x04);
lcm_dcs_write_seq_static(ctx, 0x05,0x33);       //17
lcm_dcs_write_seq_static(ctx, 0x06,0x00);      //RST rise adjust
lcm_dcs_write_seq_static(ctx, 0x07,0x00);      //RST fall adjust  20190129
lcm_dcs_write_seq_static(ctx, 0x08,0x86);    //86
lcm_dcs_write_seq_static(ctx, 0x09,0x03);    //09
lcm_dcs_write_seq_static(ctx, 0x0A,0x73);      //F7
lcm_dcs_write_seq_static(ctx, 0x0B,0x00);
lcm_dcs_write_seq_static(ctx, 0x0C,0x00);      //CLKA rise adjust
lcm_dcs_write_seq_static(ctx, 0x0D,0x00);      //CLKA rise adjust
lcm_dcs_write_seq_static(ctx, 0x0E,0x00);      //CLKA fall adjust
lcm_dcs_write_seq_static(ctx, 0x0F,0x00);      //CLKA fall adjust
lcm_dcs_write_seq_static(ctx, 0x28,0x48);      //CLKA rise adjust
lcm_dcs_write_seq_static(ctx, 0x29,0x88);      //CLKA rise adjust
lcm_dcs_write_seq_static(ctx, 0x2A,0x48);      //CLKA fall adjust
lcm_dcs_write_seq_static(ctx, 0x2B,0x88);      //CLKA fall adjust
#endif

#if 1
//FW_R
lcm_dcs_write_seq_static(ctx, 0x31,0x0C);     // RST_L
lcm_dcs_write_seq_static(ctx, 0x32,0x02);     // FW_L
lcm_dcs_write_seq_static(ctx, 0x33,0x02);     // VGL
lcm_dcs_write_seq_static(ctx, 0x34,0x23);     // VGL
lcm_dcs_write_seq_static(ctx, 0x35,0x02);     // BW_L
lcm_dcs_write_seq_static(ctx, 0x36,0x08);     // VGL_L
lcm_dcs_write_seq_static(ctx, 0x37,0x0A);     // VGL_L
lcm_dcs_write_seq_static(ctx, 0x38,0x06);     // CLK4B_L
lcm_dcs_write_seq_static(ctx, 0x39,0x06);     // CLK4_L
lcm_dcs_write_seq_static(ctx, 0x3A,0x10);     // CLK3B_L
lcm_dcs_write_seq_static(ctx, 0x3B,0x10);     // CLK3_L
lcm_dcs_write_seq_static(ctx, 0x3C,0x12);     // CLK2B_L
lcm_dcs_write_seq_static(ctx, 0x3D,0x12);     // CLK2_L
lcm_dcs_write_seq_static(ctx, 0x3E,0x14);     // CLK1B_L
lcm_dcs_write_seq_static(ctx, 0x3F,0x14);     // CLK1_L
lcm_dcs_write_seq_static(ctx, 0x40,0x16);     // STV_L
lcm_dcs_write_seq_static(ctx, 0x41,0x16);     // STV_L
lcm_dcs_write_seq_static(ctx, 0x42,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x43,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x44,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x45,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x46,0x07);     // Hi-Z
//FW_L
lcm_dcs_write_seq_static(ctx, 0x47,0x0D);     // RST_R
lcm_dcs_write_seq_static(ctx, 0x48,0x02);     // FW_R
lcm_dcs_write_seq_static(ctx, 0x49,0x02);     // VGL
lcm_dcs_write_seq_static(ctx, 0x4A,0x23);     // VGL
lcm_dcs_write_seq_static(ctx, 0x4B,0x02);     // BW_R
lcm_dcs_write_seq_static(ctx, 0x4C,0x09);     // VGL_R
lcm_dcs_write_seq_static(ctx, 0x4D,0x0B);     // VGL_R
lcm_dcs_write_seq_static(ctx, 0x4E,0x06);     // CLK4B_R
lcm_dcs_write_seq_static(ctx, 0x4F,0x06);     // CLK4_R
lcm_dcs_write_seq_static(ctx, 0x50,0x11);     // CLK3B_R
lcm_dcs_write_seq_static(ctx, 0x51,0x11);     // CLK3_R
lcm_dcs_write_seq_static(ctx, 0x52,0x13);     // CLK2B_R
lcm_dcs_write_seq_static(ctx, 0x53,0x13);     // CLK2_R
lcm_dcs_write_seq_static(ctx, 0x54,0x15);     // CLK1B_R
lcm_dcs_write_seq_static(ctx, 0x55,0x15);     // CLK1_R
lcm_dcs_write_seq_static(ctx, 0x56,0x17);     // STV_R
lcm_dcs_write_seq_static(ctx, 0x57,0x17);     // STV_R
lcm_dcs_write_seq_static(ctx, 0x58,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x59,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x5A,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x5B,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x5C,0x07);     // Hi-Z
//BW_R
lcm_dcs_write_seq_static(ctx, 0x61,0x0C);     // RST_L
lcm_dcs_write_seq_static(ctx, 0x62,0x02);     // FW_L
lcm_dcs_write_seq_static(ctx, 0x63,0x02);     // VGL
lcm_dcs_write_seq_static(ctx, 0x64,0x23);     // VGL
lcm_dcs_write_seq_static(ctx, 0x65,0x02);     // BW_L
lcm_dcs_write_seq_static(ctx, 0x66,0x08);     // VGL_L
lcm_dcs_write_seq_static(ctx, 0x67,0x0A);     // VGL_L
lcm_dcs_write_seq_static(ctx, 0x68,0x06);     // CLK4B_L
lcm_dcs_write_seq_static(ctx, 0x69,0x06);     // CLK4_L
lcm_dcs_write_seq_static(ctx, 0x6A,0x10);     // CLK3B_L
lcm_dcs_write_seq_static(ctx, 0x6B,0x10);     // CLK3_L
lcm_dcs_write_seq_static(ctx, 0x6C,0x12);     // CLK2B_L
lcm_dcs_write_seq_static(ctx, 0x6D,0x12);     // CLK2_L
lcm_dcs_write_seq_static(ctx, 0x6E,0x14);     // CLK1B_L
lcm_dcs_write_seq_static(ctx, 0x6F,0x14);     // CLK1_L
lcm_dcs_write_seq_static(ctx, 0x70,0x16);     // STV_L
lcm_dcs_write_seq_static(ctx, 0x71,0x16);     // STV_L
lcm_dcs_write_seq_static(ctx, 0x72,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x73,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x74,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x75,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x76,0x07);     // Hi-Z
//BW_L
lcm_dcs_write_seq_static(ctx, 0x77,0x0D);     // RST_R
lcm_dcs_write_seq_static(ctx, 0x78,0x02);     // FW_R
lcm_dcs_write_seq_static(ctx, 0x79,0x02);     // VGL
lcm_dcs_write_seq_static(ctx, 0x7A,0x23);     // VGL
lcm_dcs_write_seq_static(ctx, 0x7B,0x02);     // BW_R
lcm_dcs_write_seq_static(ctx, 0x7C,0x09);     // VGL_R
lcm_dcs_write_seq_static(ctx, 0x7D,0x0B);     // VGL_R
lcm_dcs_write_seq_static(ctx, 0x7E,0x06);     // CLK4B_R
lcm_dcs_write_seq_static(ctx, 0x7F,0x06);     // CLK4_R
lcm_dcs_write_seq_static(ctx, 0x80,0x11);     // CLK3B_R
lcm_dcs_write_seq_static(ctx, 0x81,0x11);     // CLK3_R
lcm_dcs_write_seq_static(ctx, 0x82,0x13);     // CLK2B_R
lcm_dcs_write_seq_static(ctx, 0x83,0x13);     // CLK2_R
lcm_dcs_write_seq_static(ctx, 0x84,0x15);     // CLK1B_R
lcm_dcs_write_seq_static(ctx, 0x85,0x15);     // CLK1_R
lcm_dcs_write_seq_static(ctx, 0x86,0x17);     // STV_R
lcm_dcs_write_seq_static(ctx, 0x87,0x17);     // STV_R
lcm_dcs_write_seq_static(ctx, 0x88,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x89,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x8A,0x07);    // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x8B,0x07);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0x8C,0x07);     // Hi-Z

lcm_dcs_write_seq_static(ctx, 0xB0,0x33);     // CLK2B_R
lcm_dcs_write_seq_static(ctx, 0xB1,0x33);     // CLK2_R
lcm_dcs_write_seq_static(ctx, 0xB2,0x00);     // CLK1B_R
lcm_dcs_write_seq_static(ctx, 0xD0,0x01);     // CLK1_R
lcm_dcs_write_seq_static(ctx, 0xD1,0x00);     // STV_R
lcm_dcs_write_seq_static(ctx, 0xE2,0x00);     // STV_R
lcm_dcs_write_seq_static(ctx, 0xE6,0x22);     // Hi-Z
lcm_dcs_write_seq_static(ctx, 0xE7,0x54);     // Hi-Z
// RTN. Internal VBP, Internal VFP
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x02);
lcm_dcs_write_seq_static(ctx, 0x06,0x8F);     // Internal Line Time (RTN)
lcm_dcs_write_seq_static(ctx, 0x0B,0xA0);     // Internal VFP[9]
lcm_dcs_write_seq_static(ctx, 0x0C,0x00);     // Internal VFP[8]
lcm_dcs_write_seq_static(ctx, 0x0D,0x14);     // Internal VBP
lcm_dcs_write_seq_static(ctx, 0x0E,0xe6);     // Internal VFP
lcm_dcs_write_seq_static(ctx, 0x4e,0x11);     // SRC BIAS
//lcm_dcs_write_seq_static(ctx, 0x4d,0xce);     // Internal VFP
lcm_dcs_write_seq_static(ctx, 0xF1,0x1C);     // Tcon ESD option
lcm_dcs_write_seq_static(ctx, 0x4B,0x5A);    // line_chopper
lcm_dcs_write_seq_static(ctx, 0x50,0xCA);    // line_chopper
lcm_dcs_write_seq_static(ctx, 0x51,0x00);     // line_chopper
//lcm_dcs_write_seq_static(ctx, 0x4E,0x11);    // SRC BIAS
//lcm_dcs_write_seq_static(ctx, 0x01,0x14);     // line_chopper
//lcm_dcs_write_seq_static(ctx, 0x02,0x0A);    // SRC BIAS
//lcm_dcs_write_seq_static(ctx, 0x4D,0xCE);    // Power Saving Off
//lcm_dcs_write_seq_static(ctx, 0x40,0x47);    //SDT
// Power Setting
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x05);
lcm_dcs_write_seq_static(ctx, 0x03,0x01); //VCOM
lcm_dcs_write_seq_static(ctx, 0x04,0x23);  //VCOM
lcm_dcs_write_seq_static(ctx, 0x58,0x61); //VGL x2
lcm_dcs_write_seq_static(ctx, 0x63,0x8D);   //GVDDN  5.3V  //5.5 97
lcm_dcs_write_seq_static(ctx, 0x64,0x8D);   //GVDDP  -5.3V
lcm_dcs_write_seq_static(ctx, 0x6A,0x79);   //VGLO  -10V
lcm_dcs_write_seq_static(ctx, 0x6B,0x6B);   //VGL   -11V
lcm_dcs_write_seq_static(ctx, 0x46,0x00);      // LVD HVREG option
lcm_dcs_write_seq_static(ctx, 0x85,0x37);      // HW RESET option
lcm_dcs_write_seq_static(ctx, 0x68,0xA1);   //VGHO   15V
lcm_dcs_write_seq_static(ctx, 0x69,0xA7);   //VGH    16V
// Resolution
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x06);
lcm_dcs_write_seq_static(ctx, 0xD9,0x1F);     // 4Lane
//lcm_dcs_write_seq_static(ctx, 0x08,0x00);     // PLL
//lcm_dcs_write_seq_static(ctx, 0xC0,0x40);     // NL = 1600
//lcm_dcs_write_seq_static(ctx, 0xC1,0x16);     // NL = 1600
lcm_dcs_write_seq_static(ctx, 0xC0,0x40);     // NL = 1600
lcm_dcs_write_seq_static(ctx, 0xC1,0x16);     // NL = 1600
#ifdef p538_19v47_gamma
//538+19v47
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x08);
lcm_dcs_write_seq_static(ctx, 0xE0,0x0,0x6a,0x9e,0xc8,0xff,0x55,0x2f,0x55,0x83,0xaa,0xa9,0xe4,0x16,0x41,0x6b,0xea,0x96,0xc8,0xe7,0xd,0xff,0x2e,0x56,0x76,0x92,0x3,0xec,);
lcm_dcs_write_seq_static(ctx, 0xE1,0x0,0x6a,0x9e,0xc8,0xff,0x55,0x2f,0x55,0x83,0xaa,0xa9,0xe4,0x16,0x41,0x6b,0xea,0x96,0xc8,0xe7,0xd,0xff,0x2e,0x56,0x76,0x92,0x3,0xec,);
#endif
// OSC Auto Trim Setting
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x0B);
lcm_dcs_write_seq_static(ctx, 0x9A,0x44);
lcm_dcs_write_seq_static(ctx, 0x9B,0x81);
lcm_dcs_write_seq_static(ctx, 0x9C,0x03);
lcm_dcs_write_seq_static(ctx, 0x9D,0x03);
lcm_dcs_write_seq_static(ctx, 0x9E,0x70);
lcm_dcs_write_seq_static(ctx, 0x9F,0x70);
lcm_dcs_write_seq_static(ctx, 0xAB,0xE0);     // AutoTrimType
//============================
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x0E);
lcm_dcs_write_seq_static(ctx, 0x00,0xA0);   //LV mode
lcm_dcs_write_seq_static(ctx, 0x11,0x10);
lcm_dcs_write_seq_static(ctx, 0x13,0x10);
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x00); //Page0
lcm_dcs_write_seq_static(ctx, 0x35,0x00); //TE enable
lcm_dcs_write_seq_static(ctx, 0xFF,0x98,0x82,0x00);
lcm_dcs_write_seq_static(ctx, 0x11);
udelay(120);
lcm_dcs_write_seq_static(ctx, 0x29);
udelay(100);
//lcm_dcs_write_seq_static(ctx, 0x51,0x3F,0xFF);//CABC
//lcm_dcs_write_seq_static(ctx, 0x53,0x2C);//模式
//lcm_dcs_write_seq_static(ctx, 0x55,0x00);
#endif
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

	pr_info("%s\n", __func__);
	if (!ctx->prepared)
		return 0;
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x81, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

#if defined(CONFIG_PRIZE_LCD_BIAS)
	//display_bias_disable();
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
	display_bias_enable_v(5800);
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

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (36)
#define HSA (8)
#define HBP (36)
#define VFP (400)
#define VSA (12)
#define VBP (8)
#define VAC (1600)
#define HAC (720)

static struct drm_display_mode default_mode = {
	.clock = 96960,//htotal*vtotal*vrefresh/1000   163943   182495
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
static struct mtk_panel_params ext_params = {
	.pll_clk = 240,
	.vfp_low_power = 400,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},

};

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
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
	    data[1] == id[1] &&
	    data[2] == id[2])
		return 1;

	pr_info("ATA expect data is %x %x %x\n", id[0], id[1], id[2]);

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

	panel->connector->display_info.width_mm = 68;
	panel->connector->display_info.height_mm = 151;//sqrt((size*25.4)^2/(18^2+9^2))*18

	return 2;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int fb_notifier_callback(struct notifier_block *nb, unsigned long event, void *data){
	struct fb_event *fb_event = data;
	int *blank;

	if (event != FB_EVENT_BLANK){
		return 0;
	}

	if (fb_event && fb_event->data) {
		blank = fb_event->data;
		printk("ili9882n Notifier's event = %ld, blank:%d truly\n", event, *blank);
		switch (*blank) {
			case FB_BLANK_POWERDOWN:
				display_bias_disable();
				break;
		}
	}

	return 0;
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

	pr_info("%s+\n", __func__);
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

	fb_nb.notifier_call = fb_notifier_callback;
	if (fb_register_client(&fb_nb)){
		dev_err(dev, "%s: cannot register fb notify\n", __func__);
	}

	pr_info("%s-\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	fb_unregister_client(&fb_nb);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "truly6517,ili9882n,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-truly6517-ili9882n-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("truly ili9882n VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
