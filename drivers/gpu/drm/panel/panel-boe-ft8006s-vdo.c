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
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(120);//ili9882n at least 10ms
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
//Stop reload
lcm_dcs_write_seq_static(ctx,0x41,0x5A);
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x24);
lcm_dcs_write_seq_static(ctx,0x90,0x5a);
//VCOM
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x03);
lcm_dcs_write_seq_static(ctx,0x80,0x72,0x00);
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x08);
lcm_dcs_write_seq_static(ctx,0x80,0xC8,0x2C,0x01);
//----------------------LCD initial code start----------------------//
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x09);
lcm_dcs_write_seq_static(ctx,0x80,0x5A,0x51,0xB5,0x2A,0x6C,0xE5,0x4A,0x01,0x40,0x62,0x0F,0x82,0x20,0x08,0xF0,0xB7);
lcm_dcs_write_seq_static(ctx,0x90,0x00,0x24,0x42,0x0A,0xE3,0x91,0xA4,0xF0,0x69,0x43,0x55,0x20,0x37,0xc1,0x26,0x00);
lcm_dcs_write_seq_static(ctx,0xA0,0x51,0x55,0x55,0x00,0xA0,0x4C,0x06,0x11,0x0D,0x60,0x00,0xFF,0xFF,0x03,0xA5,0xE6);
lcm_dcs_write_seq_static(ctx,0xB0,0x08,0x3A,0x12,0x64,0x0B,0x00,0x00,0x11,0x00,0x60,0x00,0xFF,0xFF,0x03,0xFF,0x34);
lcm_dcs_write_seq_static(ctx,0xC0,0x0C,0xFF,0x18,0x9F,0x0F,0x00,0x08,0x00);
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0A);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xFF,0x83,0x07,0x03,0x00,0x19,0x2B,0x40,0x55,0x69,0x71,0xA6,0x9D,0xD6,0x62,0x2E); 
lcm_dcs_write_seq_static(ctx,0x90,0x73,0x4A,0x47,0x37,0x27,0x1B,0x0E,0x0B,0x00,0x19,0x2B,0x40,0x55,0x69,0x71,0xA6); 
lcm_dcs_write_seq_static(ctx,0xA0,0x9D,0xD6,0x62,0x2E,0x73,0x4A,0x47,0x37,0x27,0x1B,0x0E,0x0B,0x00);       
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0B);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x20,0x44,0x08,0x00,0x60,0x47,0x00,0x00,0x10,0x22,0x04,0x00,0xB0,0x23); 
lcm_dcs_write_seq_static(ctx,0x90,0x15,0x00);                                                       
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0C);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xFA,0x68,0x68,0x01,0x32,0x37,0xD0,0x07,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51); 
lcm_dcs_write_seq_static(ctx,0x90,0x15,0x55,0x61,0x15,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51,0x15,0x55,0x61,0x95); 
lcm_dcs_write_seq_static(ctx,0xA0,0xAB,0x18,0x00,0x05,0x00,0x05,0x00,0x05,0x80,0x4C,0x29,0x84,0x52,0x01,0x09,0x00); 
lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00);                                        
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0D);                                                                     
lcm_dcs_write_seq_static(ctx,0x80,0xF0,0xB1,0x71,0xEF,0x4B,0xC0,0x80);                                         
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0E);                                            
lcm_dcs_write_seq_static(ctx,0x80,0xFF,0x01,0x55,0x55,0x32,0x88,0x88,0x1C);                                          
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0F);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xBD,0x07,0x70,0x60,0x11,0xD0,0x63,0x08,0x52,0x51,0x88,0x49,0x03,0x52,0x4C,0x4C); 
lcm_dcs_write_seq_static(ctx,0x90,0x68,0x68,0x68,0x4C,0x4C,0x7C,0x14,0x00,0x20,0x06,0xC2,0x00,0x04,0x06,0x0C,0x00); 
lcm_dcs_write_seq_static(ctx,0xA0,0x00,0x92,0x00,0x00);                           
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x10);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x03,0xE7,0x1F,0x17,0x10,0x48,0x80,0xAA,0xD0,0x18,0x30,0x88,0x41,0x8A); 
lcm_dcs_write_seq_static(ctx,0x90,0x39,0x28,0xA9,0xC5,0x9A,0x7B,0xF0,0x07,0x7E,0xE0,0x07,0x7E,0x20,0x10,0x00);      
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x11);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x46,0x77,0x03,0x40,0xCA,0xF3,0xFF,0xA3,0x20,0x08,0xC4,0x06,0xA1,0xD8,0x24,0x18); 
lcm_dcs_write_seq_static(ctx,0x90,0x30,0xC6,0x66,0xC1,0x80,0x51,0x15,0xCB,0xE5,0xD2,0x68,0x6C,0x36,0x1D,0x04,0xC8); 
lcm_dcs_write_seq_static(ctx,0xA0,0xB0,0xD9,0x88,0x60,0xB0,0x81,0x40,0x1A,0x1B,0x48,0x63,0x03,0xB9,0x00,0x1C,0x80); 
lcm_dcs_write_seq_static(ctx,0xB0,0x50,0x30,0x00,0xE0,0xE1,0x01,0x00,0x28,0x0E,0x06,0x43,0x55,0x55,0x55,0x55,0x55); 
lcm_dcs_write_seq_static(ctx,0xC0,0x95,0x88,0x88,0x88,0x88,0x88,0xC8,0x08,0x84,0xC6,0xE3,0x81,0x00,0x20,0x00,0x21); 
lcm_dcs_write_seq_static(ctx,0xD0,0x42,0x88,0x20,0x09,0x10,0x00,0x40,0x80,0x24,0x31,0x04,0x41,0x26,0x49,0x92,0x24); 
lcm_dcs_write_seq_static(ctx,0xE0,0x49,0x00,0x20,0x49,0x92,0x04,0x00,0x00,0x00,0x00,0x92,0x04,0x00,0x85,0x11,0x0C); 
lcm_dcs_write_seq_static(ctx,0xF0,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x5E,0x4A,0x01,0x78,0x00,0x08,0x00,0x00);      
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x12);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x00,0x00,0x00,0x02,0x03,0x00,0x00,0x00,0x00,0x02,0x03,0x01,0x41,0x37); 
lcm_dcs_write_seq_static(ctx,0x90,0xF1,0xE7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2D,0x23,0x05); 
lcm_dcs_write_seq_static(ctx,0xA0,0xFB,0x08,0x2D,0x23,0x05,0xFB,0x0C);                                               
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x13);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xFD,0x0F,0x00,0x0C,0x00,0x00,0x00,0x00,0x01,0x08,0x01,0x1C,0x44,0x0C,0xCE,0xE7); 
lcm_dcs_write_seq_static(ctx,0x90,0x62,0x0E,0x24,0x98,0xAC,0x21,0x01,0x00,0xD0,0x93,0x24,0x49,0x06,0x20);           
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x14);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xC1,0x02,0x41,0x36,0xE1,0xEF,0xF7,0xFB,0xFD,0x7E,0x00,0x00,0x00,0x90,0xC5,0x81); 
lcm_dcs_write_seq_static(ctx,0x90,0x40,0x20,0x20,0x06,0x04,0x02,0x01,0x1B,0xE9,0xF1,0x08,0x00,0x00,0x99,0x0C,0x04); 
lcm_dcs_write_seq_static(ctx,0xA0,0x00,0xC8,0x64,0x60,0x00,0x00,0x00,0x00,0x00,0x20,0x8B,0xA8,0x55,0x26,0x03,0x20); 
lcm_dcs_write_seq_static(ctx,0xB0,0xA2,0x49,0x99,0x0C,0x00,0x00,0x00,0x00,0x00,0x64,0x32,0x26,0x2B,0x00,0x40,0xA1); 
lcm_dcs_write_seq_static(ctx,0xC0,0x50,0x38,0x23,0x02,0x3C,0x80,0x51,0x1C,0xE7,0x43,0x80,0x07,0x30,0x8A,0xE3,0x6C); 
lcm_dcs_write_seq_static(ctx,0xD0,0x08,0xF0,0x00,0x46,0x71,0x9C,0x0B,0x01,0x1E,0xC0,0x28,0x0E,0x77,0xE1,0x7F,0xCF); 
lcm_dcs_write_seq_static(ctx,0xE0,0x19,0xC5,0xE1,0x26,0xFC,0xEF,0x19,0xA3,0x38,0xDC,0x83,0xFF,0x3D,0x63,0x14,0x87); 
lcm_dcs_write_seq_static(ctx,0xF0,0x5B,0xF0,0xBF,0x67,0x8C,0xE2,0x18,0x01,0xBF,0xDF,0x08,0x00,0x00,0x00,0x00,0x00); 
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x15);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8E,0xD9,0xE0,0xF7); 
lcm_dcs_write_seq_static(ctx,0x90,0x53,0x00,0x20,0xCB,0x44,0x2C,0x93,0x01,0x00,0x00,0x5C,0x31,0x6A,0xF1,0x41,0x0A); 
lcm_dcs_write_seq_static(ctx,0xA0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90); 
lcm_dcs_write_seq_static(ctx,0xB0,0xD5,0x12,0x53,0x28,0x4E,0xC4,0x32,0x19,0x12,0x00,0xF0,0x00,0x88,0x96,0x24,0x21); 
lcm_dcs_write_seq_static(ctx,0xC0,0xEC,0x03,0x41,0xCA,0x34,0x09,0xC0,0x5A,0xD7,0xB6,0x6D,0xDB,0xB6,0x6D,0x49,0x52); 
lcm_dcs_write_seq_static(ctx,0xD0,0xFA,0x30,0x10,0xA4,0x4C,0x53,0x30,0xAC,0x75,0x6D,0xDB,0xB6,0x6D,0xDB,0x96,0x24); 
lcm_dcs_write_seq_static(ctx,0xE0,0x25,0xEC,0x03,0x41,0xCA,0x34,0x0D,0xC1,0x5A,0xDB,0xB6,0x6D,0xDB,0xB6,0x6D,0x49); 
lcm_dcs_write_seq_static(ctx,0xF0,0x12,0xFA,0x30,0x10,0xA4,0x4C,0x13,0x20,0xAC,0xB5,0x6D,0xDB,0xB6,0x6D,0x5B,0xC0); 
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x16);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xC3,0x43,0x41,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00); 
lcm_dcs_write_seq_static(ctx,0x90,0x00,0x00,0x00,0x00,0x50,0x21);                                                    
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x18);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xEF,0xBD,0xF7,0xDE,0x7B,0xEF,0xBD,0x07,0x08,0x08,0x0A,0x0C,0x0C,0x0C,0x0C,0x0C); 
lcm_dcs_write_seq_static(ctx,0x90,0x0C,0x0C,0x0C,0x5C,0x09,0xA8,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x5A); 
lcm_dcs_write_seq_static(ctx,0xA0,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x09,0x04,0xFF,0x00,0x80); 
lcm_dcs_write_seq_static(ctx,0xB0,0x80,0x00,0x04,0x20,0x00,0x01,0x08,0x40,0x00,0x02,0x10,0x80,0x00,0x04,0x00);      
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x19);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0xC0,0xAF,0xA3,0x9B,0x92,0x8D,0x8A,0x86,0x84,0x83,0x82,0x81,0x00,0x50,0xF6,0xCF); 
lcm_dcs_write_seq_static(ctx,0x90,0xFC,0x2F,0xF3,0xEF,0xCF,0xBF,0x0F,0xFF,0xAF,0xB5,0x71,0x0E,0x6C,0x4A,0x69,0x08); 
lcm_dcs_write_seq_static(ctx,0xA0,0x00,0x00,0x08,0x00);                                                              
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x1A);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00,0x40); 
lcm_dcs_write_seq_static(ctx,0x90,0x48,0x50,0x58,0x00,0x60,0x67,0x6F,0x77,0xFC,0x80,0x88,0x90,0x98,0x00,0xA0,0xA8); 
lcm_dcs_write_seq_static(ctx,0xA0,0xB0,0xB8,0x40,0xC0,0xC8,0xD0,0xD8,0x55,0xE0,0xE8,0xF0,0xF8,0x15,0xFC,0xFE,0xFF); 
lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00); 
lcm_dcs_write_seq_static(ctx,0xC0,0x40,0x48,0x50,0x58,0x54,0x60,0x68,0x70,0x78,0x55,0x80,0x88,0x90,0x98,0x55,0xA0); 
lcm_dcs_write_seq_static(ctx,0xD0,0xA8,0xB0,0xB8,0x55,0xC0,0xC8,0xD0,0xD8,0x01,0xE0,0xE8,0xF0,0xF7,0xC0,0xFB,0xFD); 
lcm_dcs_write_seq_static(ctx,0xE0,0xFE,0x3F,0x00,0x03,0x07,0x0B,0xBC,0x0F,0x13,0x17,0x1B,0x16,0x1F,0x26,0x2E,0x36); 
lcm_dcs_write_seq_static(ctx,0xF0,0x6C,0x3D,0x45,0x4D,0x55,0x1B,0x5C,0x64,0x6C,0x74,0x1B,0x7B,0x83,0x8B,0x93,0xAF); 
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x1B);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x9B,0xA3,0xAB,0xB3,0x55,0xBB,0xC3,0xCB,0xD3,0xE9,0xDC,0xE4,0xED,0xF5,0xC8,0xFA); 
lcm_dcs_write_seq_static(ctx,0x90,0xFC,0xFE,0x0D,0x00);                                                              
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x20 );                                                                       
lcm_dcs_write_seq_static(ctx,0x80,0x7A,0x00,0x00,0x00,0x00,0x00,0x00);                                               
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x22);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x2D,0xD3,0x00,0x55,0x55,0x01,0x55,0x55,0x01,0x55,0x55,0x9F,0x00);                
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x23);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x01,0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01); 
lcm_dcs_write_seq_static(ctx,0x90,0xFF,0x0F,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0xFF,0x07,0x35 );                        
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x24);                                                                        
lcm_dcs_write_seq_static(ctx,0x80,0x00,0x03,0x00,0xFF,0xFF,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xDA,0x5A); 
lcm_dcs_write_seq_static(ctx,0x90,0x5A,0x5A,0x5A,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00); 
lcm_dcs_write_seq_static(ctx,0x42,0x24);                                                                             
lcm_dcs_write_seq_static(ctx,0x90,0x00);                                                                             
lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x2F);                                                                        
lcm_dcs_write_seq_static(ctx,0x19,0x01);                                                                             
lcm_dcs_write_seq_static(ctx,0x4C,0x03);             
lcm_dcs_write_seq_static(ctx,0x11,0x00);
mdelay(120);
lcm_dcs_write_seq_static(ctx,0x29,0x00);
mdelay(20);
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
	//lcm_dcs_write_seq_static(ctx, 0xFF, 0x98, 0x81, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(60);

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
	display_ldo18_enable(1);
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

#define HFP (65)
#define HSA (14)
#define HBP (50)
#define VFP (140)
#define VSA (8)
#define VBP (110)
#define VAC (1600)
#define HAC (720)

static struct drm_display_mode default_mode = {
	.clock = 94646,//htotal*vtotal*vrefresh/1000   163943   182495
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
	{ .compatible = "boe,ft8006s,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-ft8006s-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("truly boe ft8006s VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
