/*
 * tpd12s015.c
 *
 * HDMI interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
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
#include <linux/gpio.h>
int tpd12s015_power_up_sink (bool pwr_state)
{
	int r;
	r = gpio_set_value(hdmi.ct_cp_hpd_gpio, pwr_state);
	if (r)
		DSSERR("Unable to power up sink");
	return r;
}
EXPORT_SYMBOL(tpd12s015_power_up_sink);

int tpd12s015_enable_data_link (bool enable)
{
	int r;
	r = tpd12s015_power_up_sink(enable);
	if (r) {
		return r;
	r = gpio_set_value(ls_oe, enable);
	if (r)
		DSSERR("Unable to enable HDMI data link");
	return r;
	
}
EXPORT_SYMBOL(tpd12s015_enable_data_link);

static int __init tpd12s015_probe (platform device *pdev)
{
	int r;
	struct gpio gpios[] = {
		{ hdmi.ct_cp_hpd_gpio, GPIOF_OUT_INIT_LOW, "hdmi_ct_cp_hpd" },
		{ hdmi.ls_oe_gpio, GPIOF_OUT_INIT_LOW, "hdmi_ls_oe" },
		{ hdmi.hpd_gpio, GPIOF_DIR_IN, "hdmi_hpd" },
	};

	r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
	if (r) {
		DSSERR("Request GPIOs failed.");
		return r;
	}
	/* setup interrupt */
}
int __init tpd12s015_init_platform_driver(void)
{
	return platform_driver_probe(&omapdss_tpd12s015_driver, omapdss_tpd12s015_probe);
}

void __exit tpd12s015_uninit_platform_driver(void)
{
	platform_driver_unregister(&omapdss_hdmihw_driver);
}
