/*
 * tpd12s015.c
 *
 * HDMI interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Ricardo Neri <ricardo.neri@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <video/omapdss.h>

#define DRV_NAME "tpd12s015"
#define USE_TPD

static struct {
	int ct_cp_hpd_gpio;
	int ls_oe_gpio;
	int hpd_gpio;
} tpd12s015;

void tpd12s015_power_up_sink(bool pwr_state)
{
	gpio_set_value(tpd12s015.ct_cp_hpd_gpio, pwr_state);
}
EXPORT_SYMBOL(tpd12s015_power_up_sink);

void tpd12s015_enable_data_link(bool enable)
{
	tpd12s015_power_up_sink(enable);
	gpio_set_value(tpd12s015.ls_oe_gpio, enable);
}
EXPORT_SYMBOL(tpd12s015_enable_data_link);


void tpd12s015_detect(void)
{
}
EXPORT_SYMBOL(tpd12s015_detect);

static int __devinit tpd12s015_probe(struct platform_device *pdev)
{
	int r = 0;
#if !defined(USE_TPD)
	printk(KERN_ERR ">>>>>>>>>>>%s", __func__);
#else
	struct gpio gpios[] = {
		{ -1, GPIOF_OUT_INIT_LOW, "hdmi_ct_cp_hpd" },
		{ -1, GPIOF_OUT_INIT_LOW, "hdmi_ls_oe" },
		{ -1, GPIOF_DIR_IN, "hdmi_hpd" },
	};

	printk(KERN_ERR ">>>>>>--->>>>>%s", __func__);

	tpd12s015.ct_cp_hpd_gpio = gpios[0].gpio = 60;
	tpd12s015.ls_oe_gpio = gpios[1].gpio = 41;
	tpd12s015.hpd_gpio = gpios[2].gpio = 63;

	printk(KERN_ERR "will request [%d][%d][%d]", gpios[0].gpio, gpios[1].gpio, gpios[2].gpio);
	r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
	if (r) {
		dev_err(&pdev->dev, "Request GPIOs failed");
		return r;
	}
	/* setup interrupt */
#endif
	return r;
}

static int __devexit tpd12s015_remove(struct platform_device *pdev)
{
	printk(KERN_ERR ">>>>>>>>>>>%s", __func__);
	return 0;
}


static struct platform_driver tpd12s015_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tpd12s015_probe,
	.remove = __devexit_p(tpd12s015_remove),
};

module_platform_driver(tpd12s015_driver);

MODULE_AUTHOR("Ricardo Neri <ricardo.neri@ti.com>");
MODULE_DESCRIPTION("Driver for TPD12S015 HDMI companion chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
