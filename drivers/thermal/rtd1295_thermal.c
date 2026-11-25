// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 on-chip thermal sensor
 *
 * Each sensor is a small block of eight 32-bit registers (CPU sensor in the
 * SCPU wrapper, a second one next to the card reader).  The temperature is
 * a 19-bit two's complement value in 1/1024 degC; no calibration is applied
 * by software.
 *
 * Based on the vendor driver (drivers/thermal/realtek/sensor-rtd129x.c,
 * Copyright (C) 2018 Realtek Semiconductor Corporation, Cheng-Yu Lee).
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>

#include "thermal_hwmon.h"

#define RTD1295_TM_CTRL2		0x08
#define RTD1295_TM_STATUS1		0x18
#define RTD1295_TM_STATUS2		0x1c

/* CTRL2 values used by the vendor driver to (re)start the sensor */
#define RTD1295_TM_CTRL2_RESET		0x01904001
#define RTD1295_TM_CTRL2_RUN		0x01924001

#define RTD1295_TM_TEMP_SIGN_BIT	18
#define RTD1295_TM_STATUS2_BAD		0x3fffff

struct rtd1295_thermal {
	struct device *dev;
	void __iomem *base;
};

static void rtd1295_thermal_reset(struct rtd1295_thermal *priv)
{
	writel(RTD1295_TM_CTRL2_RESET, priv->base + RTD1295_TM_CTRL2);
	writel(RTD1295_TM_CTRL2_RUN, priv->base + RTD1295_TM_CTRL2);
	usleep_range(25000, 26000);
}

static int rtd1295_thermal_read(struct rtd1295_thermal *priv)
{
	u32 val = readl(priv->base + RTD1295_TM_STATUS1);

	/* 1/1024 degC -> millidegC */
	return sign_extend32(val, RTD1295_TM_TEMP_SIGN_BIT) * 1000 / 1024;
}

static bool rtd1295_thermal_status_bad(struct rtd1295_thermal *priv)
{
	u32 val = readl(priv->base + RTD1295_TM_STATUS2);

	return val == 0 || val == RTD1295_TM_STATUS2_BAD;
}

static int rtd1295_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct rtd1295_thermal *priv = thermal_zone_device_priv(tz);
	int t = rtd1295_thermal_read(priv);

	/* The sensor occasionally wedges; the vendor driver restarts it. */
	if (t < -3000 || t > 150000 || rtd1295_thermal_status_bad(priv)) {
		dev_dbg(priv->dev, "restarting sensor (temp %d)\n", t);
		rtd1295_thermal_reset(priv);
		t = rtd1295_thermal_read(priv);
		if (t < -3000 || t > 150000 ||
		    rtd1295_thermal_status_bad(priv))
			return -EIO;
	}

	*temp = t;
	return 0;
}

static const struct thermal_zone_device_ops rtd1295_thermal_ops = {
	.get_temp = rtd1295_thermal_get_temp,
};

static int rtd1295_thermal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_zone_device *tz;
	struct rtd1295_thermal *priv;
	struct resource *res;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = dev;

	/*
	 * Not requested exclusively: both sensors sit inside register
	 * blocks owned by other devices (SCPU wrapper, card reader).
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	priv->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->base)
		return -ENOMEM;

	rtd1295_thermal_reset(priv);

	tz = devm_thermal_of_zone_register(dev, 0, priv, &rtd1295_thermal_ops);
	if (IS_ERR(tz))
		return dev_err_probe(dev, PTR_ERR(tz),
				     "failed to register thermal zone\n");

	if (devm_thermal_add_hwmon_sysfs(dev, tz))
		dev_warn(dev, "failed to add hwmon sysfs attributes\n");

	return 0;
}

static const struct of_device_id rtd1295_thermal_of_match[] = {
	{ .compatible = "realtek,rtd1295-thermal" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1295_thermal_of_match);

static struct platform_driver rtd1295_thermal_driver = {
	.probe = rtd1295_thermal_probe,
	.driver = {
		.name = "rtd1295-thermal",
		.of_match_table = rtd1295_thermal_of_match,
	},
};
module_platform_driver(rtd1295_thermal_driver);

MODULE_DESCRIPTION("Realtek RTD1295 thermal sensor driver");
MODULE_LICENSE("GPL");
