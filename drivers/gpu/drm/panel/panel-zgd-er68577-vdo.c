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
	//gpiod_set_value(ctx->reset_gpio, 0);
	//udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(5);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

lcm_dcs_write_seq_static(ctx,0xE0,0xAB,0xBA);
lcm_dcs_write_seq_static(ctx,0xE1,0xBA,0xAB);
lcm_dcs_write_seq_static(ctx,0xB1,0x10,0x01,0x47,0xFF);
lcm_dcs_write_seq_static(ctx,0xB2,0x0C,0x14,0x04,0x50,0x50,0x14);//#VBP,VFP,VSW,HBP,HFP,HSW);
lcm_dcs_write_seq_static(ctx,0xB3,0x56,0x52,0xA0);
lcm_dcs_write_seq_static(ctx,0xB4,0x2A,0x30,0x04);
lcm_dcs_write_seq_static(ctx,0x00,0xB5,0x00);
lcm_dcs_write_seq_static(ctx,0xB6,0x00,0x00,0x00,0x10,0x00,0x10,0x00);
lcm_dcs_write_seq_static(ctx,0xB7,0x0E,0x00,0xFF,0x08,0x08,0xFF,0xFF,0x00);
lcm_dcs_write_seq_static(ctx,0xB8,0x05,0x12,0x29,0x49,0x48,0x00,0x00);

//#,,,,,,,,,,,,,,,,,,,,,255,,251,,247,,243,,235,,227,,211,,191,,159,,128,,096,,064,,044,,028,,020,,012,,008,,004,,000,,255,,251,,247,,243,,235,,227,,211,,191,,159,,128,,096,,064,,044,,028,,020,,012,,008,,004,,000
lcm_dcs_write_seq_static(ctx,0xB9,0x7C,0x5a,0x46,0x36,0x30,0x20,0x25,0x11,0x2f,0x31,0x34,0x55,0x45,0x50,0x44,0x46,0x3e,0x36,0x06,0x7C,0x5a,0x46,0x36,0x30,0x20,0x25,0x11,0x2f,0x31,0x34,0x55,0x45,0x50,0x44,0x46,0x3e,0x36,0x06);
lcm_dcs_write_seq_static(ctx,0xBA,0x00,0x00,0x00,0x44,0x24,0x00,0x00,0x00);//,,#,GAMMA,OP,SET
lcm_dcs_write_seq_static(ctx,0xBB,0x76,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xBC,0x00,0x00);//,,,,,#,MTP_AUTO_PROG
lcm_dcs_write_seq_static(ctx,0xBD,0xFF,0x00,0x00,0x00,0x00);//,,#,MTP_MANUAL
lcm_dcs_write_seq_static(ctx,0x00,0xBE,0x00);//,,,,,,#,MTP_READ,DATA
lcm_dcs_write_seq_static(ctx,0xC0,0x54,0x32,0x23,0x45,0x44,0x44,0x44,0x44,0x90,0x04,0x90,0x04,0x0F,0x00,0x00,0xc1);
lcm_dcs_write_seq_static(ctx,0xC1,0x33,0x94,0x02,0x87,0x90,0x04,0x90,0x04,0x54,0x00);
lcm_dcs_write_seq_static(ctx,0xC2,0x73,0x09,0x08,0x89,0x08,0x11,0x22,0x33,0x44,0x82,0x18,0x00);
lcm_dcs_write_seq_static(ctx,0xC3,0x8B,0x49,0x13,0x11,0x0F,0x0D,0x00,0x00,0x00,0x02,0x00,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x05,0x07,0x02,0x02);//,#Forward,scan
lcm_dcs_write_seq_static(ctx,0xC4,0x0a,0x08,0x12,0x10,0x0E,0x0C,0x00,0x00,0x00,0x02,0x00,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x04,0x06,0x02,0x02);//,#Forward,scan
lcm_dcs_write_seq_static(ctx,0xC5,0xE8,0x85,0x76);//,,,#ID,SET,FOR,MTP
lcm_dcs_write_seq_static(ctx,0xC6,0x20,0x20);//,,,#VCOM

lcm_dcs_write_seq_static(ctx,0xC7,0x41,0x01,0x0D,0x11,0x09,0x15,0x19,0x4F,0x10,0xD7,0xCF,0x19,0x1B,0x1D,0x03,0x02,0x25,0x30,0x00,0x03,0xFF,0x00);
lcm_dcs_write_seq_static(ctx,0xC8,0x31,0x00,0x31,0x42,0x34,0x16);
lcm_dcs_write_seq_static(ctx,0xC9,0xA1,0x22,0xFF,0xC4,0x23);
lcm_dcs_write_seq_static(ctx,0xCA,0xCB,0x43);
lcm_dcs_write_seq_static(ctx,0xCC,0x2E,0x02,0x04,0x08);
lcm_dcs_write_seq_static(ctx,0xCD,0x0E,0x59,0x59,0x20,0x19,0x6B,0x06,0x93);
lcm_dcs_write_seq_static(ctx,0xD0,0x07,0x10,0x80);
lcm_dcs_write_seq_static(ctx,0xD1,0x00,0x0D,0xFF,0x0F);
lcm_dcs_write_seq_static(ctx,0xD2,0xE3,0x2B,0x38,0x00);
lcm_dcs_write_seq_static(ctx,0xD3,0x00,0x00,0x00,0x00,0x00,0x33,0x20,0x39,0xD5,0x86,0xF3);
lcm_dcs_write_seq_static(ctx,0xD4,0x00,0x01,0x00,0x0E,0x04,0x44,0x08,0x10,0x00,0x07,0x00);
lcm_dcs_write_seq_static(ctx,0x00,0xD5,0x00);
lcm_dcs_write_seq_static(ctx,0xD6,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xD7,0x00,0x00,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0x00,0xE2,0x20);
lcm_dcs_write_seq_static(ctx,0xE4,0x08,0x55,0x03);
lcm_dcs_write_seq_static(ctx,0xE6,0x00,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF);//,,,#AUTO,VPORCH 
lcm_dcs_write_seq_static(ctx,0xE7,0x00,0x00,0x00);//,,,#MTP,RELOAD
lcm_dcs_write_seq_static(ctx,0xE8,0xD5,0xFF,0xFF,0xFF,0x00,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0x00,0xE9,0xFF);
lcm_dcs_write_seq_static(ctx,0xF0,0x12,0x03,0x20,0x00,0xFF);//,,#,PWM,CTL

lcm_dcs_write_seq_static(ctx,0xF1,0xA6,0xC8,0xEA,0xE6,0xE4,0xCC,0xE4,0xBE,0xF0,0xB2,0xAA,0xC7,0xFF,0x66,0x98,0xE3,0x87,0xC8,0x99,0xC8,0x8C,0xBE,0x96,0x91,0x8F,0xFF);
lcm_dcs_write_seq_static(ctx,0x00,0xF3,0x00);
lcm_dcs_write_seq_static(ctx,0xFA,0x00,0x84,0x12,0x21,0x48,0x48,0x21,0x12,0x84,0x69,0x69,0x5A,0xA5,0x96,0x96,0xA5,0x5A,0xB7,0xDE,0xED,0x7B,0x7B,0xED,0xDE,0xB7);
lcm_dcs_write_seq_static(ctx,0xFB,0x00,0x12,0x0F,0xFF,0xFF,0xFF,0x00,0x38,0x40,0x08,0x70,0x0B,0x40,0x19,0x50,0x21,0xC0,0x27,0x60,0x2D,0x00,0x00,0x0F);

lcm_dcs_write_seq_static(ctx,0x11,0);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x29,0);
msleep(20);
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
	display_bias_disable();
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
	msleep(20);

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
	msleep(10);
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

	msleep(10);
	msleep(10);
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

#define HFP (80)
#define HSA (20)
#define HBP (80)
#define VFP (20)
#define VSA (4)
#define VBP (12)
#define VAC (1440)
#define HAC (720)
static const struct drm_display_mode default_mode = {
	.clock = 163944,     //htotal*vtotal*vrefresh/1000   163943   182495
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
	.pll_clk = 240,
	.vfp_low_power = 450,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
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
	panel->connector->display_info.height_mm = 154;//sqrt((size*25.4)^2/(1600^2+720^2))*1600

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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO
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
	{ .compatible = "zgd,er68577,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-zgd-er68577-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("truly td4330 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
