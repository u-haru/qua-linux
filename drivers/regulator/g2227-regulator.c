// SPDX-License-Identifier: GPL-2.0-only
/*
 * GMT G2227 PMIC regulator driver
 *
 * The G2227 has six buck converters and two LDOs behind a plain 8-bit I2C
 * register map. Only the normal-mode settings are handled; the sleep-mode
 * voltages and modes are left as the boot loader set them.
 *
 * Based on the Realtek vendor kernel's G2227 driver,
 * Copyright (C) 2016-2018 Realtek Semiconductor Corporation.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define G2227_REG_ONOFF		0x05
#define G2227_REG_MODE(n)	(0x07 + (n))	/* 0x07..0x0a */
#define G2227_REG_DCDC2_NVO	0x10
#define G2227_REG_DCDC3_NVO	0x11
#define G2227_REG_DCDC5_NVO	0x12
#define G2227_REG_DCDC1_6_NVO	0x13
#define G2227_REG_LDO_NVO	0x14
#define G2227_REG_VERSION	0x20

#define G2227_CHIP_ID_MASK	GENMASK(7, 3)
#define G2227_CHIP_ID		0x1b
#define G2227_VERSION_MASK	GENMASK(2, 0)

#define G2227_DCDC_VSEL_MASK	GENMASK(4, 0)

/* Normal-mode field values, also used by regulator-initial-mode */
#define G2227_DC_MODE_AUTO	0
#define G2227_DC_MODE_FORCE_PWM	1
#define G2227_LDO_MODE_NORMAL	0
#define G2227_LDO_MODE_ECO	2

enum g2227_id {
	G2227_DCDC1,
	G2227_DCDC2,
	G2227_DCDC3,
	G2227_DCDC4,
	G2227_DCDC5,
	G2227_DCDC6,
	G2227_LDO2,
	G2227_LDO3,
	G2227_NUM_REGULATORS,
};

/*
 * Each rail has a 2-bit normal-mode field: two rails per register, the
 * first one in bits 7:6, the second one in bits 3:2, in the order of
 * enum g2227_id.
 */
static void g2227_mode_field(const struct regulator_desc *desc,
			     unsigned int *reg, unsigned int *shift)
{
	*reg = G2227_REG_MODE(desc->id / 2);
	*shift = (desc->id & 1) ? 2 : 6;
}

static int g2227_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	const struct regulator_desc *desc = rdev->desc;
	bool ldo = desc->id >= G2227_LDO2;
	unsigned int reg, shift, val;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = ldo ? G2227_LDO_MODE_NORMAL : G2227_DC_MODE_AUTO;
		break;
	case REGULATOR_MODE_FAST:
		if (ldo)
			return -EINVAL;
		val = G2227_DC_MODE_FORCE_PWM;
		break;
	case REGULATOR_MODE_IDLE:
		if (!ldo)
			return -EINVAL;
		val = G2227_LDO_MODE_ECO;
		break;
	default:
		return -EINVAL;
	}

	g2227_mode_field(desc, &reg, &shift);
	return regmap_update_bits(rdev->regmap, reg, 0x3 << shift,
				  val << shift);
}

static unsigned int g2227_get_mode(struct regulator_dev *rdev)
{
	const struct regulator_desc *desc = rdev->desc;
	unsigned int reg, shift, val;
	int ret;

	g2227_mode_field(desc, &reg, &shift);
	ret = regmap_read(rdev->regmap, reg, &val);
	if (ret)
		return REGULATOR_MODE_INVALID;
	val = (val >> shift) & 0x3;

	if (desc->id >= G2227_LDO2)
		return val == G2227_LDO_MODE_ECO ? REGULATOR_MODE_IDLE :
						   REGULATOR_MODE_NORMAL;

	return val == G2227_DC_MODE_FORCE_PWM ? REGULATOR_MODE_FAST :
						REGULATOR_MODE_NORMAL;
}

static unsigned int g2227_dc_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case G2227_DC_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	case G2227_DC_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static unsigned int g2227_ldo_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case G2227_LDO_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case G2227_LDO_MODE_ECO:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const struct regulator_ops g2227_linear_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.set_mode		= g2227_set_mode,
	.get_mode		= g2227_get_mode,
};

static const struct regulator_ops g2227_table_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_table,
	.map_voltage		= regulator_map_voltage_iterate,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_mode		= g2227_set_mode,
	.get_mode		= g2227_get_mode,
};

static const struct regulator_ops g2227_fixed_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.set_mode		= g2227_set_mode,
	.get_mode		= g2227_get_mode,
};

static const unsigned int g2227_ldo_volt_table[] = {
	 800000,  850000,  900000,  950000, 1000000, 1100000, 1200000, 1300000,
	1500000, 1600000, 1800000, 1900000, 2500000, 2600000, 3000000, 3100000,
};

#define G2227_COMMON(_id, _name)					\
	.name		= _name,					\
	.of_match	= of_match_ptr(_name),				\
	.regulators_node = of_match_ptr("regulators"),			\
	.id		= G2227_##_id,					\
	.type		= REGULATOR_VOLTAGE,				\
	.owner		= THIS_MODULE,					\
	.enable_reg	= G2227_REG_ONOFF,				\
	.enable_mask	= BIT(7 - G2227_##_id)

/* 800 mV to 1187.5 mV in 12.5 mV steps */
#define G2227_DCDC(_id, _name, _vreg)					\
	{								\
		G2227_COMMON(_id, _name),				\
		.ops		= &g2227_linear_ops,			\
		.of_map_mode	= g2227_dc_of_map_mode,			\
		.n_voltages	= 32,					\
		.min_uV		= 800000,				\
		.uV_step	= 12500,				\
		.vsel_reg	= _vreg,				\
		.vsel_mask	= G2227_DCDC_VSEL_MASK,			\
	}

#define G2227_LDO(_id, _name, _vmask)					\
	{								\
		G2227_COMMON(_id, _name),				\
		.ops		= &g2227_table_ops,			\
		.of_map_mode	= g2227_ldo_of_map_mode,		\
		.volt_table	= g2227_ldo_volt_table,			\
		.n_voltages	= ARRAY_SIZE(g2227_ldo_volt_table),	\
		.vsel_reg	= G2227_REG_LDO_NVO,			\
		.vsel_mask	= _vmask,				\
	}

static const struct regulator_desc g2227_regulators[] = {
	/* 3.0 V to 3.3 V in 100 mV steps */
	{
		G2227_COMMON(DCDC1, "dcdc1"),
		.ops		= &g2227_linear_ops,
		.of_map_mode	= g2227_dc_of_map_mode,
		.n_voltages	= 4,
		.min_uV		= 3000000,
		.uV_step	= 100000,
		.vsel_reg	= G2227_REG_DCDC1_6_NVO,
		.vsel_mask	= GENMASK(7, 6),
	},
	G2227_DCDC(DCDC2, "dcdc2", G2227_REG_DCDC2_NVO),
	G2227_DCDC(DCDC3, "dcdc3", G2227_REG_DCDC3_NVO),
	/* fixed 1.8 V */
	{
		G2227_COMMON(DCDC4, "dcdc4"),
		.ops		= &g2227_fixed_ops,
		.of_map_mode	= g2227_dc_of_map_mode,
		.n_voltages	= 1,
		.min_uV		= 1800000,
		.fixed_uV	= 1800000,
	},
	G2227_DCDC(DCDC5, "dcdc5", G2227_REG_DCDC5_NVO),
	G2227_DCDC(DCDC6, "dcdc6", G2227_REG_DCDC1_6_NVO),
	G2227_LDO(LDO2, "ldo2", GENMASK(7, 4)),
	G2227_LDO(LDO3, "ldo3", GENMASK(3, 0)),
};

static bool g2227_writeable_reg(struct device *dev, unsigned int reg)
{
	return reg != 0x00 && reg != G2227_REG_VERSION;
}

static bool g2227_volatile_reg(struct device *dev, unsigned int reg)
{
	return reg == G2227_REG_VERSION;
}

static const struct regmap_config g2227_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= G2227_REG_VERSION,
	.writeable_reg	= g2227_writeable_reg,
	.volatile_reg	= g2227_volatile_reg,
	.cache_type	= REGCACHE_MAPLE,
};

static int g2227_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct regmap *regmap;
	unsigned int val;
	int i, ret;

	regmap = devm_regmap_init_i2c(client, &g2227_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap),
				     "failed to init regmap\n");

	/*
	 * On the au Qua Station the PMIC NAKs its address on the first
	 * transfer after every boot, however late that transfer comes; the
	 * next one works. Resetting the I2C controller or toggling the pin
	 * mux at runtime does not bring it back, so the bus is probably left
	 * in a bad state by the boot chain. Retry a few times.
	 */
	for (i = 0; i < 3; i++) {
		ret = regmap_read(regmap, G2227_REG_VERSION, &val);
		if (!ret)
			break;
		usleep_range(1000, 2000);
	}
	if (ret)
		return dev_err_probe(dev, ret, "failed to read chip id\n");
	if (i)
		dev_info(dev, "chip id read failed %d time(s) before succeeding\n",
			 i);

	if (FIELD_GET(G2227_CHIP_ID_MASK, val) != G2227_CHIP_ID)
		return dev_err_probe(dev, -ENODEV,
				     "unexpected chip id 0x%02x\n", val);

	dev_info(dev, "G2227 version %lu\n",
		 FIELD_GET(G2227_VERSION_MASK, val));

	config.dev = dev;
	config.regmap = regmap;

	for (i = 0; i < ARRAY_SIZE(g2227_regulators); i++) {
		rdev = devm_regulator_register(dev, &g2227_regulators[i],
					       &config);
		if (IS_ERR(rdev))
			return dev_err_probe(dev, PTR_ERR(rdev),
					     "failed to register %s\n",
					     g2227_regulators[i].name);
	}

	return 0;
}

static const struct of_device_id g2227_of_match[] = {
	{ .compatible = "gmt,g2227" },
	{ }
};
MODULE_DEVICE_TABLE(of, g2227_of_match);

static const struct i2c_device_id g2227_i2c_ids[] = {
	{ "g2227" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, g2227_i2c_ids);

static struct i2c_driver g2227_driver = {
	.driver = {
		.name = "g2227-regulator",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = g2227_of_match,
	},
	.probe = g2227_probe,
	.id_table = g2227_i2c_ids,
};
module_i2c_driver(g2227_driver);

MODULE_DESCRIPTION("GMT G2227 PMIC regulator driver");
MODULE_LICENSE("GPL");
