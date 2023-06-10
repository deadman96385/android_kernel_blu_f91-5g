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

//----------------------LCD initial code start----------------------//
//============CMD WR enable=============
//CMD2 WR enable
lcm_dcs_write_seq_static(ctx,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x80,0x09,0x01);

lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xFF,0x80,0x09);
//======voltage set==========
lcm_dcs_write_seq_static(ctx,0x00,0x93);//VGH_N 20.5V
lcm_dcs_write_seq_static(ctx,0xC5,0x91);

lcm_dcs_write_seq_static(ctx,0x00,0x97);//VGH_I 20.5V
lcm_dcs_write_seq_static(ctx,0xC5,0x91);

lcm_dcs_write_seq_static(ctx,0x00,0x9E);
lcm_dcs_write_seq_static(ctx,0xC5,0x0A); //2AVDD-2AVEE

lcm_dcs_write_seq_static(ctx,0x00,0x9A); //VGL_N -11.5V 
lcm_dcs_write_seq_static(ctx,0xC5,0xBC); 

lcm_dcs_write_seq_static(ctx,0x00,0x9C); //VGL_I -11.5V 2AVEE-AVDD
lcm_dcs_write_seq_static(ctx,0xC5,0xBC);

lcm_dcs_write_seq_static(ctx,0x00,0xB6);
lcm_dcs_write_seq_static(ctx,0xC5,0x64,0x64);  //VGHO1_N_I 16V

lcm_dcs_write_seq_static(ctx,0x00,0xB8);
lcm_dcs_write_seq_static(ctx,0xC5,0x37,0x37); //VGLO1_N_I -11V

lcm_dcs_write_seq_static(ctx,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xD8,0x2B,0x2B); //GVDDP/N 5.0V -5.0V

lcm_dcs_write_seq_static(ctx,0x00,0x01);
lcm_dcs_write_seq_static(ctx,0xD9,0x9A,0x9A);  //#VCOM=-2.24V 

lcm_dcs_write_seq_static(ctx,0x00,0x82);
lcm_dcs_write_seq_static(ctx,0xC5,0x15); //LVD

lcm_dcs_write_seq_static(ctx,0x00,0x83);
lcm_dcs_write_seq_static(ctx,0xC5,0x07); //LVD Enable

lcm_dcs_write_seq_static(ctx,0x00,0xB3); //Set reg_hvreg_en_vglo1_sink=0
lcm_dcs_write_seq_static(ctx,0xC5,0xC7);

lcm_dcs_write_seq_static(ctx,0x00,0xB0); //Set reg_hvreg_en_vgho1_sink=0
lcm_dcs_write_seq_static(ctx,0xC5,0xC7);

lcm_dcs_write_seq_static(ctx,0x00,0xA9);
lcm_dcs_write_seq_static(ctx,0xCF,0x56);

lcm_dcs_write_seq_static(ctx,0x00,0x85); //dis vdd5/vref_tp 
lcm_dcs_write_seq_static(ctx,0xF5,0x00);

//reg_sd_sap
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xA4,0x8C);

//Panel Set
lcm_dcs_write_seq_static(ctx,0x00,0xA6);
lcm_dcs_write_seq_static(ctx,0xB3,0xF2);

//SD_BLANK_FRAME:
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xC4,0x15);

lcm_dcs_write_seq_static(ctx,0x00,0xC9);
lcm_dcs_write_seq_static(ctx,0xF5,0x04);

//==========gamma==============
//Analog Gamma 
lcm_dcs_write_seq_static(ctx,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xE1,0x0A,0x12,0x1F,0x28,0x31,0x38,0x43,0x4E,0x51,0x5F,0x62,0x6D,0x96,0x7B,0x7C,0x72);
lcm_dcs_write_seq_static(ctx,0x00,0x10);
lcm_dcs_write_seq_static(ctx,0xE1,0x6D,0x64,0x59,0x52,0x4C,0x40,0x30,0x14);

lcm_dcs_write_seq_static(ctx,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0xE2,0x00,0x12,0x1F,0x28,0x31,0x38,0x43,0x4E,0x51,0x5F,0x62,0x7D,0x86,0x7B,0x7C,0x72);
lcm_dcs_write_seq_static(ctx,0x00,0x10);
lcm_dcs_write_seq_static(ctx,0xE2,0x6D,0x64,0x59,0x52,0x4C,0x40,0x30,0x0A);



//==============GOA  setting==================================================
//Norm Power-OFF LVD//
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xCB,0xC1,0xC1,0xFD,0xC1,0xFD,0xC1,0xFD);

lcm_dcs_write_seq_static(ctx,0x00,0x87);
lcm_dcs_write_seq_static(ctx,0xCB,0xFD);

lcm_dcs_write_seq_static(ctx,0x00,0x88);
lcm_dcs_write_seq_static(ctx,0xCB,0xFD,0xFD,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1);

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xCB,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1);

lcm_dcs_write_seq_static(ctx,0x00,0x97);
lcm_dcs_write_seq_static(ctx,0xCB,0xFD,0xFD);

//Power-On//
lcm_dcs_write_seq_static(ctx,0x00,0x99);
lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0xA0);
lcm_dcs_write_seq_static(ctx,0xCB,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0xA1);
lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0xA9);
lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0xB0);
lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00);


//POWROF BLANK//
lcm_dcs_write_seq_static(ctx,0x00,0xB9);
lcm_dcs_write_seq_static(ctx,0xCB,0x04,0x45,0x50,0x00,0x00,0x01,0x00);

//SKIP//
lcm_dcs_write_seq_static(ctx,0x00,0xB2);
lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

//Power-on/off blank frame number//
lcm_dcs_write_seq_static(ctx,0x00,0xCB);
lcm_dcs_write_seq_static(ctx,0xC0,0x11);


//U2D//
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xCC,0x3F,0x3F,0x3F,0x3F,0x3F,0x05,0x03,0x0B);

lcm_dcs_write_seq_static(ctx,0x00,0x88);
lcm_dcs_write_seq_static(ctx,0xCC,0x0D,0x0F,0x11,0x13,0x15,0x17,0x19,0x2F);

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xCC,0x2F,0x1B,0x1D,0x3F,0x3F,0x3F); 
    
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xCD,0x3F,0x3F,0x3F,0x3F,0x3F,0x04,0x02,0x0A);

lcm_dcs_write_seq_static(ctx,0x00,0x88);
lcm_dcs_write_seq_static(ctx,0xCD,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x2F);

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xCD,0x2F,0x1A,0x1C,0x3F,0x3F,0x3F);
 
//D2U//
lcm_dcs_write_seq_static(ctx,0x00,0xA0); 
lcm_dcs_write_seq_static(ctx,0xCC,0x3F,0x3F,0x3F,0x3F,0x3F,0x1A,0x1C,0x10); 

lcm_dcs_write_seq_static(ctx,0x00,0xA8);
lcm_dcs_write_seq_static(ctx,0xCC,0x0E,0x0C,0x0A,0x18,0x16,0x14,0x12,0x2F);

lcm_dcs_write_seq_static(ctx,0x00,0xB0); 
lcm_dcs_write_seq_static(ctx,0xCC,0x2F,0x04,0x02,0x3F,0x3F,0x3F); 
    
lcm_dcs_write_seq_static(ctx,0x00,0xA0);
lcm_dcs_write_seq_static(ctx,0xCD,0x3F,0x3F,0x3F,0x3F,0x3F,0x1B,0x1D,0x11);

lcm_dcs_write_seq_static(ctx,0x00,0xA8); 
lcm_dcs_write_seq_static(ctx,0xCD,0x0F,0x0D,0x0B,0x19,0x17,0x15,0x13,0x2F); 

lcm_dcs_write_seq_static(ctx,0x00,0xB0); 
lcm_dcs_write_seq_static(ctx,0xCD,0x2F,0x05,0x03,0x3F,0x3F,0x3F); 



//VST1~VST4//
lcm_dcs_write_seq_static(ctx,0x00,0x80);//VST1
lcm_dcs_write_seq_static(ctx,0xC2,0x8F,0x09,0x00,0x20); 

lcm_dcs_write_seq_static(ctx,0x00,0x84);//VST2
lcm_dcs_write_seq_static(ctx,0xC2,0x8E,0x09,0x00,0x20); 

lcm_dcs_write_seq_static(ctx,0x00,0x88);//VST3
lcm_dcs_write_seq_static(ctx,0xC2,0x8D,0x09,0x00,0x20); 

lcm_dcs_write_seq_static(ctx,0x00,0x8C);//VST4
lcm_dcs_write_seq_static(ctx,0xC2,0x8C,0x09,0x00,0x20); 


//VEND1~VEND4//
lcm_dcs_write_seq_static(ctx,0x00,0xA0);//VEND1
lcm_dcs_write_seq_static(ctx,0xC2,0x86,0x08,0x48,0x84); 

lcm_dcs_write_seq_static(ctx,0x00,0xA4);//VEND2
lcm_dcs_write_seq_static(ctx,0xC2,0x85,0x08,0x48,0x84);

lcm_dcs_write_seq_static(ctx,0x00,0xA8);//VEND3
lcm_dcs_write_seq_static(ctx,0xC2,0x82,0x08,0x48,0x84);

lcm_dcs_write_seq_static(ctx,0x00,0xAC);//VEND4
lcm_dcs_write_seq_static(ctx,0xC2,0x81,0x08,0x48,0x84);


//CLKA//  
lcm_dcs_write_seq_static(ctx,0x00,0xC0); 
lcm_dcs_write_seq_static(ctx,0xC2,0x8A,0x01,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xC7); 
lcm_dcs_write_seq_static(ctx,0xC2,0x89,0x02,0x00,0x06,0x00,0x98,0x0F); 

lcm_dcs_write_seq_static(ctx,0x00,0xD0); 
lcm_dcs_write_seq_static(ctx,0xC2,0x88,0x03,0x00,0x06,0x00,0x98,0x0F); 

lcm_dcs_write_seq_static(ctx,0x00,0xD7); 
lcm_dcs_write_seq_static(ctx,0xC2,0x87,0x04,0x00,0x06,0x00,0x98,0x0F); 

//CLKB//  
lcm_dcs_write_seq_static(ctx,0x00,0xE0);
lcm_dcs_write_seq_static(ctx,0xC2,0x86,0x05,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xE7);
lcm_dcs_write_seq_static(ctx,0xC2,0x85,0x06,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xF0);
lcm_dcs_write_seq_static(ctx,0xC2,0x84,0x07,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xF7);
lcm_dcs_write_seq_static(ctx,0xC2,0x83,0x08,0x00,0x06,0x00,0x98,0x0F);

//CLKC//  
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xC3,0x82,0x09,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0x87);
lcm_dcs_write_seq_static(ctx,0xC3,0x81,0x0A,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xC3,0x00,0x0B,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0x97);
lcm_dcs_write_seq_static(ctx,0xC3,0x01,0x0C,0x00,0x06,0x00,0x98,0x0F);

//CLKD//  
lcm_dcs_write_seq_static(ctx,0x00,0xC0);
lcm_dcs_write_seq_static(ctx,0xCD,0x02,0x0D,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xC7);
lcm_dcs_write_seq_static(ctx,0xCD,0x03,0x0E,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xD0);
lcm_dcs_write_seq_static(ctx,0xCD,0x04,0x0F,0x00,0x06,0x00,0x98,0x0F);

lcm_dcs_write_seq_static(ctx,0x00,0xD7);
lcm_dcs_write_seq_static(ctx,0xCD,0x05,0x10,0x00,0x06,0x00,0x98,0x0F);


 
//======Long V mode=====================
//FIFO Mode
lcm_dcs_write_seq_static(ctx,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0x1C,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xC0,0x00,0xC0,0x00,0xC8,0x00,0x0A); 

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xC0,0x00,0xC0,0x00,0xC8,0x00,0x0A); 

lcm_dcs_write_seq_static(ctx,0x00,0xB0);
lcm_dcs_write_seq_static(ctx,0xC0,0x00,0xC0,0x00,0xC8,0x0A); 

lcm_dcs_write_seq_static(ctx,0x00,0xA3);
lcm_dcs_write_seq_static(ctx,0xC1,0x00,0x04,0x00,0x04);

//LH Disable
lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xCE,0x00);

lcm_dcs_write_seq_static(ctx,0x00,0xA0);
lcm_dcs_write_seq_static(ctx,0xC1,0xE0);

lcm_dcs_write_seq_static(ctx,0x00,0xD1);
lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x0A,0x01,0x01,0x00,0xA7,0x01); 

lcm_dcs_write_seq_static(ctx,0x00,0xE6);
lcm_dcs_write_seq_static(ctx,0xCE,0x00,0x5E); 

//VB Enable
lcm_dcs_write_seq_static(ctx,0x00,0xD0);
lcm_dcs_write_seq_static(ctx,0xCE,0x01);

//VB_TERM1B_HI
//lcm_dcs_write_seq_static(ctx,0x00,0xD5);
//lcm_dcs_write_seq_static(ctx,0xCE,0x00);

//VB_TERM1B_LO
//lcm_dcs_write_seq_static(ctx,0x00,0xD6);
//lcm_dcs_write_seq_static(ctx,0xCE,0xCC);

//RTN auto caliibration off
lcm_dcs_write_seq_static(ctx,0x00,0xE0);
lcm_dcs_write_seq_static(ctx,0xCE,0x00);

//Term2 auto calibration off
lcm_dcs_write_seq_static(ctx,0x00,0xF0);
lcm_dcs_write_seq_static(ctx,0xCE,0x00);

//RTN
//lcm_dcs_write_seq_static(ctx,0x00,0x81);
//lcm_dcs_write_seq_static(ctx,0xC0,0xBD);

//front porch
//lcm_dcs_write_seq_static(ctx,0x00,0x83);
//lcm_dcs_write_seq_static(ctx,0xC0,0xF0);

//Back porch
//lcm_dcs_write_seq_static(ctx,0x00,0x85);
//lcm_dcs_write_seq_static(ctx,0xC0,0x0A);


 

//======Source set=====================
//reg_sd_zigzag_set
lcm_dcs_write_seq_static(ctx,0x00,0x8C);
lcm_dcs_write_seq_static(ctx,0xC4,0x10);
//Z_B
//=========mirror_x2=============
lcm_dcs_write_seq_static(ctx,0x00,0xE6);
lcm_dcs_write_seq_static(ctx,0xC0,0x40);

//=========cclr_bgr=============
lcm_dcs_write_seq_static(ctx,0x00,0xA3);
lcm_dcs_write_seq_static(ctx,0xF3,0x80);

lcm_dcs_write_seq_static(ctx,0x00,0xA3);
lcm_dcs_write_seq_static(ctx,0xC4,0x00);//FW

//=========even_odd_swap_line=============
lcm_dcs_write_seq_static(ctx,0x00,0xB4);
lcm_dcs_write_seq_static(ctx,0xC4,0x6C,0x6C,0x6C,0x6C,0x6C,0x6C,0x6C,0x6C);

//=========data_shift=============
lcm_dcs_write_seq_static(ctx,0x00,0xC8);
lcm_dcs_write_seq_static(ctx,0xC4,0x20,0x20,0x20,0x20);


//=========tcon_pre_disp=============
//lcm_dcs_write_seq_static(ctx,0x00,0xE7);
//lcm_dcs_write_seq_static(ctx,0xC0,0x03);

//=========rd_order_pre_shift=============
//lcm_dcs_write_seq_static(ctx,0x00,0xD0);
//lcm_dcs_write_seq_static(ctx,0xC4,0x00,0x02);

//=========rd_order_point_1_2=============
//lcm_dcs_write_seq_static(ctx,0x00,0xD2);
//lcm_dcs_write_seq_static(ctx,0xC4,0x00,0x03,0x00,0x07);

//=========rd_order_point_3=============
//lcm_dcs_write_seq_static(ctx,0x00,0xD6);
//lcm_dcs_write_seq_static(ctx,0xC4,0x06,0x17);

//=========rd_order_point_4=============
//lcm_dcs_write_seq_static(ctx,0x00,0xDE);
//lcm_dcs_write_seq_static(ctx,0xC4,0x06,0x1B);

//=========rd_order_cycle=============
//lcm_dcs_write_seq_static(ctx,0x00,0xD8);
//lcm_dcs_write_seq_static(ctx,0xC4,0x8F,0xF0);

//=========lr_dummy_range=============
//lcm_dcs_write_seq_static(ctx,0x00,0xDA);
//lcm_dcs_write_seq_static(ctx,0xC4,0x00,0x06,0x06,0x1C);

//=========rd_order_set_15=============
//lcm_dcs_write_seq_static(ctx,0x00,0xE0);
//lcm_dcs_write_seq_static(ctx,0xC4,0x03,0x13,0x01,0x00,0x00,0x00,0x00,0x00);

//=========rd_order_set_31=============
//lcm_dcs_write_seq_static(ctx,0x00,0xE8);
//lcm_dcs_write_seq_static(ctx,0xC4,0x03,0x13,0x01,0x00,0x03,0x13,0x01,0x00);

//=========rd_order_set_47=============
//lcm_dcs_write_seq_static(ctx,0x00,0xF0);
//lcm_dcs_write_seq_static(ctx,0xC4,0x03,0x13,0x01,0x00,0x03,0x13,0x01,0x00);

//=========dum_line=============

//=========pre_read_signal=============
//lcm_dcs_write_seq_static(ctx,0x00,0xF8);
//lcm_dcs_write_seq_static(ctx,0xC4,0x1C,0x1E);

//=========cycle_read=============
//lcm_dcs_write_seq_static(ctx,0x00,0xFA);
//lcm_dcs_write_seq_static(ctx,0xC4,0xDC,0x1E);

//=========sync_write_bank=============
//lcm_dcs_write_seq_static(ctx,0x00,0xFC);
//lcm_dcs_write_seq_static(ctx,0xC4,0x03,0x07,0xAA,0x00);

//=========OD Set===========//
lcm_dcs_write_seq_static(ctx,0x00,0xB7);
lcm_dcs_write_seq_static(ctx,0xF3,0x01);

//=========OD==========//
//=========voltage od=============
lcm_dcs_write_seq_static(ctx,0x00,0x81);
lcm_dcs_write_seq_static(ctx,0xC6,0x00);//0x80:OD Enable);0x00:OD Disable

//=========od range set=============
lcm_dcs_write_seq_static(ctx,0x00,0x82);
lcm_dcs_write_seq_static(ctx,0xC6,0x00,0x11,0x11,0x00,0x00,0x00,0x00,0x00);

//=========od range set=============
lcm_dcs_write_seq_static(ctx,0x00,0x8A);
lcm_dcs_write_seq_static(ctx,0xC6,0x00); 
     


//===========SW_PANSET============
//720
lcm_dcs_write_seq_static(ctx,0x00,0xA1);
lcm_dcs_write_seq_static(ctx,0xB3,0x02,0xD0);

//1640
lcm_dcs_write_seq_static(ctx,0x00,0xA3);
lcm_dcs_write_seq_static(ctx,0xB3,0x06,0x68);


//================================
// Gate EQ 
//lcm_dcs_write_seq_static(ctx,0x00,0xB6);
//lcm_dcs_write_seq_static(ctx,0xC3,0x0F,0xF0,0x33,0x0F,0xF0,0x33);

//lcm_dcs_write_seq_static(ctx,0x00,0xC7);
//lcm_dcs_write_seq_static(ctx,0xC3,0x0F,0xF0,0x33,0x0F,0xF0,0x33);


lcm_dcs_write_seq_static(ctx,0x00,0xA0);
lcm_dcs_write_seq_static(ctx,0xF3,0x01);//OSC Auto calibration

lcm_dcs_write_seq_static(ctx,0x00,0x90);
lcm_dcs_write_seq_static(ctx,0xC4,0x98);  //CLK Source delay

lcm_dcs_write_seq_static(ctx,0x00,0x80);
lcm_dcs_write_seq_static(ctx,0xC1,0x22,0x22);//OSC
//----------------------LCD initial code End----------------------//

lcm_dcs_write_seq_static(ctx,0x11,0);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x29,0);
lcm_dcs_write_seq_static(ctx,0x35,0x01);
msleep(50);
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
#define HSA (16)
#define HBP (16)
#define VFP (200)
#define VSA (2)
#define VBP (8)
#define VAC (1640)
#define HAC (720)
static const struct drm_display_mode default_mode = {
	.clock = 92352,     //htotal*vtotal*vrefresh/1000   163943   182495
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

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 290,
	.vfp_low_power = 450,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9C,
	},
#if 0
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
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

	panel->connector->display_info.width_mm = 71;
	panel->connector->display_info.height_mm = 156;//sqrt((size*25.4)^2/(1600^2+720^2))*1600

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
	{ .compatible = "ivo,ft8009ab,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-ivo-ft8009ab-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("truly td4330 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
