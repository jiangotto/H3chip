// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ILI9488 LCD Controller
 *
 * Copyright (C) 2014 Noralf Tronnes
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME		"fb_ili9488"
#define WIDTH		320
#define HEIGHT		480

/* this init sequence matches PiScreen */
static const s16 default_init_sequence[] = {
    /* Positive Gamma Control  */
	-1, 0xE0, 0x00, 0x07, 0x0f, 0x0D, 0x1B, 0x0A, 0x3c, 0x78, 0x4A, 0x07, 0x0E, 0x09, 0x1B, 0x1e, 0x0F,
	/* Negative Gamma Control  */
	-1, 0xE1, 0x00, 0x22, 0x24, 0x06, 0x12, 0x07, 0x36, 0x47, 0x47, 0x06, 0x0a, 0x07, 0x30, 0x37, 0x0F,
	/* Power Control 1  */
	-1, 0xC0, 0x10, 0x10,
	/* Power Control 2  */
	-1, 0xC1, 0x41,
	/* VCOM Control  */
	-1, 0xC5, 0x00, 0x22, 0x80,
	// /* Memory Access Control */
	// -1, 0x36, 0x48,
	/* Pixel Interface Format colour for SPI  0x66 18 bit*/
	-1, 0x3A, 0x66,
	/* Interface Mode Control */
	-1, 0xB0, 0x00,
	/* Frame Rate Control */
	-1, 0xB1, 0xB0, 0x11,
	/* Display Inversion Control */
	-1, 0xB4, 0x02,
	/* Display Function Control */
	-1, 0xB6, 0x02, 0x02, 
	/* Entry Mode Set */
	-1, 0xB7, 0xC6,
    -1, 0xE9, 0x00,
	/* Adjust Control 3 */
	-1, 0xF7, 0xA9, 0x51, 0x2C, 0x82,
	/* Exit Sleep */
	-1, 0x11,
	-2, 120,
	/* Display on */
	-1, 0x29,
	-2, 25,
	/* end marker */
	-3
    
	// /* Interface Mode Control */
	// -1, 0xb0, 0x0,
	// -1, MIPI_DCS_EXIT_SLEEP_MODE,
	// -2, 250,
	// /* Interface Pixel Format */
	// -1, MIPI_DCS_SET_PIXEL_FORMAT, 0x55,
	// /* Power Control 3 */
	// -1, 0xC2, 0x44,
	// /* VCOM Control 1 */
	// -1, 0xC5, 0x00, 0x00, 0x00, 0x00,
	// /* PGAMCTRL(Positive Gamma Control) */
	// -1, 0xE0, 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
	// 	  0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00,
	// /* NGAMCTRL(Negative Gamma Control) */
	// -1, 0xE1, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
	// 	  0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	// /* Digital Gamma Control 1 */
	// -1, 0xE2, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
	// 	  0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	// -1, MIPI_DCS_EXIT_SLEEP_MODE,
	// -1, MIPI_DCS_SET_DISPLAY_ON,
	// /* end marker */
	// -3
};

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	write_reg(par, 0x2a,
		  xs >> 8, xs, xe >> 8, xe);

	write_reg(par, 0x2b,
		  ys >> 8, ys, ye >> 8, ye);

	write_reg(par, 0x2c);
}

/* 16 bit convert to 18 bit pixel over 8-bit databus */
static int write_vmem16_18bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16;
	u8 *txbuf = par->txbuf.buf;
	size_t remain;
	size_t to_copy;
	size_t tx_array_size;
	int i;
	int ret = 0;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	/* remaining number of pixels to send */
	remain = len / 2;
	vmem16 = (u16 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpiod_set_value(par->gpio.dc, 1);

	/* number of pixels that fits in the transmit buffer */
	tx_array_size = par->txbuf.len / 3;

	while (remain) {
		/* number of pixels to copy in one iteration of the loop */
		to_copy = min(tx_array_size, remain);
		dev_dbg(par->info->device, "    to_copy=%zu, remain=%zu\n",
						to_copy, remain - to_copy);

		for (i = 0; i < to_copy; i++) {
			u16 pixel = vmem16[i];
			u16 b = pixel & 0x1f;
			u16 g = (pixel & (0x3f << 5)) >> 5;
			u16 r = (pixel & (0x1f << 11)) >> 11;

			u8 r8 = (r & 0x1F) << 3;
			u8 g8 = (g & 0x3F) << 2;
			u8 b8 = (b & 0x1F) << 3;

			txbuf[i * 3 + 0] = r8;
			txbuf[i * 3 + 1] = g8;
			txbuf[i * 3 + 2] = b8;
		}

		vmem16 = vmem16 + to_copy;
		ret = par->fbtftops.write(par, par->txbuf.buf, to_copy * 3);
		if (ret < 0)
			return ret;
		remain -= to_copy;
	}
	return ret;
}

static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x36,
			  0x80 | (par->bgr << 3));
		break;
	case 90:
		write_reg(par, 0x36,
			  0x20 | (par->bgr << 3));
		break;
	case 180:
		write_reg(par, 0x36,
			  0x20 | (par->bgr << 3));
		break;
	case 270:
		write_reg(par, 0x36,
			  0xE0 | (par->bgr << 3));
		break;
	default:
		break;
	}

	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.init_sequence = default_init_sequence,
	.fbtftops = {
		.set_addr_win = set_addr_win,
		.set_var = set_var,
    .write_vmem = write_vmem16_18bus8,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "ilitek,ili9488", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ili9488");
MODULE_ALIAS("platform:ili9488");

MODULE_DESCRIPTION("FB driver for the ILI9488 LCD Controller");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");