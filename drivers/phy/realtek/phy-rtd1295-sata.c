// SPDX-License-Identifier: GPL-2.0
/*
 * Realtek RTD1295 SATA PHY driver
 *
 * Based on the Realtek BSP driver phy-rtk-sata.c by Simon Hsu.
 *
 * The PHY is programmed through an indirect write port located in the
 * AHCI register block (0x9803ff60). Each 32-bit word written to the data
 * register carries the PHY register address, the SATA generation it applies
 * to and the value. A few enable bits live in the SB2 block, and the
 * parameter set depends on the chip revision (SB2 0x204).
 *
 * The resets (SATA MAC, PHY, PHY power) and the drive power supply are
 * handled by the AHCI controller node, which releases them before it calls
 * phy_init().
 */

#include <linux/array_size.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#define SATA_PHY_DATA		0x0
#define SATA_PHY_PORT		0x4
#define SATA_PHY_SPEED		0x8
#define SATA_PHY_SPEED_DEFAULT	0xa	/* BSP "speed-limit" 0, no limit */

#define SB2_SATA_PHY_CTRL	0x980
#define SB2_SATA_PHY0_EN	(BIT(0) | BIT(2) | BIT(4) | BIT(8))
#define SB2_CHIP_REV		0x204
#define  SB2_CHIP_REV_MASK	GENMASK(31, 16)	/* rest of the word: not rev */

#define RTD1295_CHIP_REV_A00	0x00000000
#define RTD1295_CHIP_REV_A01	0x00010000
#define RTD1295_CHIP_REV_B00	0x00020000

#define TX_DRIVING_DEFAULT	3

static const u32 rtd1295_sata_phy_param[] = {
	0x00001111, 0x00005111, 0x00009111,
	0x336a0511, 0x336a4511, 0x336a8511,
	0xe0700111, 0xe05c4111, 0xe04a8111,
	0x00150611, 0x00154611, 0x00158611,
	0xc6000a11, 0xc6004a11, 0xc6008a11,
	0x70000211, 0x70004211, 0x70008211,
	0xc6600a11, 0xc6604a11, 0xc6608a11,
	0x20041911, 0x20045911, 0x20049911,
	0x94aa2011, 0x94aa6011, 0x94aaa011,
	0x17171511, 0x17175511, 0x17179511,
	0x07701611, 0x07705611, 0x07709611,
	0x40002a11, 0x40006a11, 0x4000aa11,
	0x29001011, 0x29005011, 0x29009011,
	0x40000c11, 0x40004c11, 0x40008c11,
	0x00271711, 0x00275711, 0x00279711,
};

/* TX swing, indexed by the "realtek,tx-driving" level */
static const u32 rtd1295_sata_phy_txdrv[][6] = {
	/* max 500 mV */
	{ 0x94a52011, 0x94a56011, 0x94a5a011,
	  0x385a2111, 0x385a6111, 0x385aa111 },
	/* max 573 mV */
	{ 0x94a62011, 0x94a66011, 0x94a6a011,
	  0x486a2111, 0x486a6111, 0x486aa111 },
	/* max 683 mV */
	{ 0x94a72011, 0x94a76011, 0x94a7a011,
	  0x587a2111, 0x587a6111, 0x587aa111 },
	/* max 786 mV */
	{ 0x94aa2011, 0x94aa6011, 0x94aaa011,
	  0x88aa2111, 0x88aa6111, 0x88aaa111 },
};

/* Spread spectrum clocking disabled */
static const u32 rtd1295_sata_phy_ssc_off[] = {
	0x538e0411, 0x538e4411, 0x538e8411,
};

static const u32 rtd1295_sata_phy_sens_a0x[] = {
	0x72100911, 0x72104911, 0x72108911,
	0x27730311, 0x27684311, 0x27688311,
};

static const u32 rtd1295_sata_phy_sens_b0x[] = {
	0x42100911, 0x42104911, 0x42108911,
	0x276f0311, 0x276d4311, 0x276d8311,
};

/* B00 only */
static const u32 rtd1295_sata_phy_clk_delay[] = {
	0x7c002a11, 0x7c006a11, 0x7c00aa11,
};

struct rtd1295_sata_phy {
	struct device *dev;
	void __iomem *base;
	struct regmap *sb2;
	u32 tx_driving;
};

static void rtd1295_sata_phy_write(struct rtd1295_sata_phy *priv, u32 reg,
				   u32 val)
{
	writel(val, priv->base + reg);
	/* The BSP waits 1 ms after every access */
	fsleep(1000);
}

static void rtd1295_sata_phy_load(struct rtd1295_sata_phy *priv,
				  const u32 *table, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		rtd1295_sata_phy_write(priv, SATA_PHY_DATA, table[i]);
}

static int rtd1295_sata_phy_init(struct phy *phy)
{
	struct rtd1295_sata_phy *priv = phy_get_drvdata(phy);
	const u32 *sens = rtd1295_sata_phy_sens_b0x;
	u32 rev;
	int ret;

	ret = regmap_read(priv->sb2, SB2_CHIP_REV, &rev);
	if (ret)
		return ret;
	rev &= SB2_CHIP_REV_MASK;

	ret = regmap_set_bits(priv->sb2, SB2_SATA_PHY_CTRL, SB2_SATA_PHY0_EN);
	if (ret)
		return ret;

	/* Select PHY 0 */
	rtd1295_sata_phy_write(priv, SATA_PHY_PORT, 0);

	rtd1295_sata_phy_load(priv, rtd1295_sata_phy_param,
			      ARRAY_SIZE(rtd1295_sata_phy_param));
	rtd1295_sata_phy_load(priv, rtd1295_sata_phy_txdrv[priv->tx_driving],
			      ARRAY_SIZE(rtd1295_sata_phy_txdrv[0]));
	rtd1295_sata_phy_load(priv, rtd1295_sata_phy_ssc_off,
			      ARRAY_SIZE(rtd1295_sata_phy_ssc_off));

	if (rev == RTD1295_CHIP_REV_A00 || rev == RTD1295_CHIP_REV_A01)
		sens = rtd1295_sata_phy_sens_a0x;
	rtd1295_sata_phy_load(priv, sens, ARRAY_SIZE(rtd1295_sata_phy_sens_b0x));

	if (rev == RTD1295_CHIP_REV_B00)
		rtd1295_sata_phy_load(priv, rtd1295_sata_phy_clk_delay,
				      ARRAY_SIZE(rtd1295_sata_phy_clk_delay));

	rtd1295_sata_phy_write(priv, SATA_PHY_SPEED, SATA_PHY_SPEED_DEFAULT);

	dev_dbg(priv->dev, "PHY initialized (chip rev 0x%08x)\n", rev);

	return 0;
}

static int rtd1295_sata_phy_exit(struct phy *phy)
{
	struct rtd1295_sata_phy *priv = phy_get_drvdata(phy);

	return regmap_clear_bits(priv->sb2, SB2_SATA_PHY_CTRL, SB2_SATA_PHY0_EN);
}

static const struct phy_ops rtd1295_sata_phy_ops = {
	.init = rtd1295_sata_phy_init,
	.exit = rtd1295_sata_phy_exit,
	.owner = THIS_MODULE,
};

static int rtd1295_sata_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct rtd1295_sata_phy *priv;
	struct resource *res;
	struct clk *clk;
	struct phy *phy;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = dev;

	/*
	 * The PHY window sits inside the AHCI register block, which the AHCI
	 * driver requests, so map it without requesting the region.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	priv->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->base)
		return -ENOMEM;

	priv->sb2 = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,sb2");
	if (IS_ERR(priv->sb2))
		return dev_err_probe(dev, PTR_ERR(priv->sb2),
				     "failed to get SB2 syscon\n");

	clk = devm_clk_get_optional_enabled(dev, NULL);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "failed to get clock\n");

	priv->tx_driving = TX_DRIVING_DEFAULT;
	device_property_read_u32(dev, "realtek,tx-driving", &priv->tx_driving);
	if (priv->tx_driving >= ARRAY_SIZE(rtd1295_sata_phy_txdrv)) {
		dev_warn(dev, "invalid tx-driving %u, using %u\n",
			 priv->tx_driving, TX_DRIVING_DEFAULT);
		priv->tx_driving = TX_DRIVING_DEFAULT;
	}

	phy = devm_phy_create(dev, NULL, &rtd1295_sata_phy_ops);
	if (IS_ERR(phy))
		return dev_err_probe(dev, PTR_ERR(phy), "failed to create PHY\n");
	phy_set_drvdata(phy, priv);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id rtd1295_sata_phy_of_match[] = {
	{ .compatible = "realtek,rtd1295-sata-phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1295_sata_phy_of_match);

static struct platform_driver rtd1295_sata_phy_driver = {
	.probe = rtd1295_sata_phy_probe,
	.driver = {
		.name = "phy-rtd1295-sata",
		.of_match_table = rtd1295_sata_phy_of_match,
	},
};
module_platform_driver(rtd1295_sata_phy_driver);

MODULE_DESCRIPTION("Realtek RTD1295 SATA PHY driver");
MODULE_LICENSE("GPL");
