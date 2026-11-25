// SPDX-License-Identifier: GPL-2.0
/*
 * Realtek RTD1295 RLE0599 USB 2.0 PHY (EHCI/OHCI port)
 *
 * The PHY behind the RTD1295 EHCI/OHCI pair is reached through the EHCI
 * vendor register INSNREG05 (vendor control/status port) plus a "VStatusOut2"
 * data register in the EHCI/OHCI wrapper. The word just before it in the
 * wrapper gates the OHCI clock; the vendor BSP forces it on before any OHCI
 * register access, so this driver does that in init/power_on.
 *
 * Parameter tables are the ones of the vendor BSP (4.9 rtd-129x-usb.dtsi).
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

/* wrapper (reg 0) */
#define RLE0599_WRAP_OHCI_CTRL		0x0
#define  RLE0599_OHCI_CLK_EN		0x40
#define RLE0599_WRAP_VSTATUS		0x4

/* EHCI INSNREG05 (reg 1) */
#define INSNREG05_VBUSY			BIT(17)
#define INSNREG05_VPORT1		BIT(13)
#define INSNREG05_VLOADM		BIT(12)
#define INSNREG05_VCTRL_SHIFT		8

#define RLE0599_PAGE_SEL		0xf4
#define RLE0599_PAGE0			0x9b
#define RLE0599_PAGE1			0xbb

#define RLE0599_P0_E7			0xe7
#define  RLE0599_P0_E7_DISC_MASK	(BIT(4) | BIT(5) | BIT(6))
#define RLE0599_P1_E0			0xe0
#define  RLE0599_P1_E0_TOGGLE		BIT(2)

struct rle0599_reg {
	u8 addr;
	u8 val;
};

static const struct rle0599_reg rle0599_page0[] = {
	{ 0xe0, 0x90 }, { 0xe1, 0x30 }, { 0xe2, 0x3a }, { 0xe3, 0x8d },
	{ 0xe4, 0xa9 }, { 0xe5, 0x65 }, { 0xe6, 0x91 }, { 0xe7, 0x81 },
	{ 0xf0, 0xfc }, { 0xf1, 0x8c }, { 0xf2, 0x00 }, { 0xf3, 0x11 },
	{ 0xf5, 0x81 }, { 0xf6, 0x00 }, { 0xf7, 0x02 },
};

static const struct rle0599_reg rle0599_page1[] = {
	{ 0xe0, 0x25 }, { 0xe1, 0xef }, { 0xe2, 0x60 }, { 0xe3, 0x00 },
	{ 0xe4, 0x00 }, { 0xe5, 0x0f }, { 0xe6, 0x18 }, { 0xe7, 0xe3 },
};

#define RLE0599_P0_E7_DEFAULT		0x81
#define RLE0599_P1_E0_DEFAULT		0x25

struct rle0599_phy {
	void __iomem *wrap;
	void __iomem *insnreg05;
};

static int rle0599_vctrl(struct rle0599_phy *p, u32 val)
{
	u32 tmp;

	writel(val, p->insnreg05);
	return readl_poll_timeout(p->insnreg05, tmp, !(tmp & INSNREG05_VBUSY),
				  10, 10000);
}

static int rle0599_write(struct rle0599_phy *p, u8 addr, u8 data)
{
	u8 nibble[2] = { addr & 0x0f, addr >> 4 };
	u32 tmp, val;
	int i, ret;

	writel(data, p->wrap + RLE0599_WRAP_VSTATUS);
	ret = readl_poll_timeout(p->insnreg05, tmp, !(tmp & INSNREG05_VBUSY),
				 10, 10000);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(nibble); i++) {
		val = INSNREG05_VPORT1 | INSNREG05_VLOADM |
		      (nibble[i] << INSNREG05_VCTRL_SHIFT);
		ret = rle0599_vctrl(p, val);
		if (ret)
			return ret;
		ret = rle0599_vctrl(p, val & ~INSNREG05_VLOADM);
		if (ret)
			return ret;
	}

	return 0;
}

static int rle0599_write_page(struct rle0599_phy *p, u8 page,
			      const struct rle0599_reg *tbl, int n)
{
	int i, ret;

	ret = rle0599_write(p, RLE0599_PAGE_SEL, page);
	for (i = 0; !ret && i < n; i++)
		ret = rle0599_write(p, tbl[i].addr, tbl[i].val);

	return ret;
}

static void rle0599_ohci_clk_on(struct rle0599_phy *p)
{
	if (!readl(p->wrap + RLE0599_WRAP_OHCI_CTRL))
		writel(RLE0599_OHCI_CLK_EN, p->wrap + RLE0599_WRAP_OHCI_CTRL);
}

/* Re-arm the disconnect detector and pulse page1 0xe0 bit 2. */
static int rle0599_toggle(struct rle0599_phy *p, bool connect)
{
	u8 e7 = RLE0599_P0_E7_DEFAULT;
	int ret;

	if (connect)
		e7 &= ~RLE0599_P0_E7_DISC_MASK;
	else
		e7 |= RLE0599_P0_E7_DISC_MASK;

	ret = rle0599_write(p, RLE0599_PAGE_SEL, RLE0599_PAGE0);
	if (!ret)
		ret = rle0599_write(p, RLE0599_P0_E7, e7);
	if (!ret)
		ret = rle0599_write(p, RLE0599_PAGE_SEL, RLE0599_PAGE1);
	if (!ret)
		ret = rle0599_write(p, RLE0599_P1_E0,
				    RLE0599_P1_E0_DEFAULT & ~RLE0599_P1_E0_TOGGLE);
	if (ret)
		return ret;
	usleep_range(1000, 1500);

	return rle0599_write(p, RLE0599_P1_E0, RLE0599_P1_E0_DEFAULT);
}

static int rle0599_init(struct phy *phy)
{
	struct rle0599_phy *p = phy_get_drvdata(phy);
	int ret;

	rle0599_ohci_clk_on(p);

	ret = rle0599_write_page(p, RLE0599_PAGE0, rle0599_page0,
				 ARRAY_SIZE(rle0599_page0));
	if (!ret)
		ret = rle0599_write_page(p, RLE0599_PAGE1, rle0599_page1,
					 ARRAY_SIZE(rle0599_page1));
	if (!ret)
		ret = rle0599_toggle(p, false);
	if (ret)
		dev_err(&phy->dev, "PHY access timed out\n");

	return ret;
}

static int rle0599_power_on(struct phy *phy)
{
	rle0599_ohci_clk_on(phy_get_drvdata(phy));

	return 0;
}

static int rle0599_connect(struct phy *phy, int port)
{
	return rle0599_toggle(phy_get_drvdata(phy), true);
}

static int rle0599_disconnect(struct phy *phy, int port)
{
	return rle0599_toggle(phy_get_drvdata(phy), false);
}

static const struct phy_ops rle0599_ops = {
	.init		= rle0599_init,
	.power_on	= rle0599_power_on,
	.connect	= rle0599_connect,
	.disconnect	= rle0599_disconnect,
	.owner		= THIS_MODULE,
};

static int rle0599_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct rle0599_phy *p;
	struct resource *res;
	struct phy *phy;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->wrap = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(p->wrap))
		return PTR_ERR(p->wrap);

	/* INSNREG05 lives inside the EHCI register block: map, don't request */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;
	p->insnreg05 = devm_ioremap(dev, res->start, resource_size(res));
	if (!p->insnreg05)
		return -ENOMEM;

	phy = devm_phy_create(dev, NULL, &rle0599_ops);
	if (IS_ERR(phy))
		return PTR_ERR(phy);
	phy_set_drvdata(phy, p);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id rle0599_of_match[] = {
	{ .compatible = "realtek,rtd1295-rle0599-phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, rle0599_of_match);

static struct platform_driver rle0599_driver = {
	.probe	= rle0599_probe,
	.driver	= {
		.name		= "phy-rtd1295-rle0599",
		.of_match_table	= rle0599_of_match,
	},
};
module_platform_driver(rle0599_driver);

MODULE_DESCRIPTION("Realtek RTD1295 RLE0599 USB 2.0 PHY driver");
MODULE_LICENSE("GPL");
