// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 true random number generator
 *
 * Based on the vendor driver (drivers/char/hw_random/rtd-rng.c,
 * Copyright (c) 2015-2016 Realtek).
 */

#include <linux/delay.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/minmax.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#define RTD1295_RNG_ANA			0x14
#define RTD1295_RNG_CALI_CHK		0x18
#define RTD1295_RNG_LOCK_CHK		0x24
#define RTD1295_RNG_RETURN3		0x2c
#define RTD1295_RNG_RESULTR		0x38

#define RTD1295_RNG_RETURN3_VALID	BIT(0)

/* Analog/calibration settings from the vendor driver */
#define RTD1295_RNG_CHK_VAL		0xd8000220
#define RTD1295_RNG_ANA_VAL		0x00008891

#define RTD1295_RNG_TIMEOUT_US		1000

/*
 * Entropy credited per 1024 bits read. The TRNG has no public
 * characterisation and the analog settings are copied blind from the vendor
 * driver, so claim only a quarter bit per bit (the hwrng default of 1024
 * would credit it as perfect). It can be changed at run time through
 * /sys/class/misc/hw_random/rng_quality.
 */
#define RTD1295_RNG_QUALITY		256

struct rtd1295_rng {
	struct hwrng rng;
	void __iomem *base;
};

static int rtd1295_rng_init(struct hwrng *rng)
{
	struct rtd1295_rng *priv = container_of(rng, struct rtd1295_rng, rng);

	writel(RTD1295_RNG_CHK_VAL, priv->base + RTD1295_RNG_CALI_CHK);
	writel(RTD1295_RNG_CHK_VAL, priv->base + RTD1295_RNG_LOCK_CHK);
	writel(RTD1295_RNG_ANA_VAL, priv->base + RTD1295_RNG_ANA);
	usleep_range(2000, 3000);

	return 0;
}

static int rtd1295_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct rtd1295_rng *priv = container_of(rng, struct rtd1295_rng, rng);
	size_t done = 0;
	u32 val;
	int ret;

	while (done < max) {
		if (wait)
			ret = readl_poll_timeout(priv->base + RTD1295_RNG_RETURN3,
						 val,
						 val & RTD1295_RNG_RETURN3_VALID,
						 10, RTD1295_RNG_TIMEOUT_US);
		else
			ret = (readl(priv->base + RTD1295_RNG_RETURN3) &
			       RTD1295_RNG_RETURN3_VALID) ? 0 : -EAGAIN;
		if (ret)
			break;

		val = readl(priv->base + RTD1295_RNG_RESULTR);
		memcpy(buf + done, &val, min(max - done, sizeof(val)));
		done += min(max - done, sizeof(val));
	}

	if (!done && wait)
		return -ETIMEDOUT;

	return done;
}

static int rtd1295_rng_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtd1295_rng *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->rng.name = dev_name(dev);
	priv->rng.init = rtd1295_rng_init;
	priv->rng.read = rtd1295_rng_read;
	priv->rng.quality = RTD1295_RNG_QUALITY;

	ret = devm_hwrng_register(dev, &priv->rng);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register hwrng\n");

	return 0;
}

static const struct of_device_id rtd1295_rng_of_match[] = {
	{ .compatible = "realtek,rtd1295-rng" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1295_rng_of_match);

static struct platform_driver rtd1295_rng_driver = {
	.probe = rtd1295_rng_probe,
	.driver = {
		.name = "rtd1295-rng",
		.of_match_table = rtd1295_rng_of_match,
	},
};
module_platform_driver(rtd1295_rng_driver);

MODULE_DESCRIPTION("Realtek RTD1295 random number generator driver");
MODULE_LICENSE("GPL");
