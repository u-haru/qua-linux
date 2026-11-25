// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 eFuse (read-only)
 *
 * The 1 KiB fuse array is mirrored as plain memory-mapped registers once
 * the boot ROM has loaded it, so reading is a simple MMIO copy.  Accesses
 * are done as aligned 32-bit reads.
 *
 * Based on the vendor driver (drivers/nvmem/rtk-efuse.c,
 * Copyright (C) 2016-2018 Realtek Semiconductor Corporation, Cheng-Yu Lee).
 */

#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>

struct rtd1295_efuse {
	void __iomem *base;
};

static int rtd1295_efuse_read(void *context, unsigned int offset, void *val,
			      size_t bytes)
{
	struct rtd1295_efuse *priv = context;
	u8 *buf = val;

	while (bytes--) {
		u32 word = readl(priv->base + (offset & ~3U));

		*buf++ = word >> (8 * (offset & 3));
		offset++;
	}

	return 0;
}

static int rtd1295_efuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_config config = { };
	struct rtd1295_efuse *priv;
	struct nvmem_device *nvmem;
	struct resource *res;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	config.dev = dev;
	config.name = "rtd1295-efuse";
	config.id = NVMEM_DEVID_NONE;
	config.owner = THIS_MODULE;
	config.read_only = true;
	config.root_only = true;
	config.stride = 1;
	config.word_size = 1;
	config.size = resource_size(res);
	config.reg_read = rtd1295_efuse_read;
	config.priv = priv;

	nvmem = devm_nvmem_register(dev, &config);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id rtd1295_efuse_of_match[] = {
	{ .compatible = "realtek,rtd1295-efuse" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1295_efuse_of_match);

static struct platform_driver rtd1295_efuse_driver = {
	.probe = rtd1295_efuse_probe,
	.driver = {
		.name = "rtd1295-efuse",
		.of_match_table = rtd1295_efuse_of_match,
	},
};
module_platform_driver(rtd1295_efuse_driver);

MODULE_DESCRIPTION("Realtek RTD1295 eFuse driver");
MODULE_LICENSE("GPL");
