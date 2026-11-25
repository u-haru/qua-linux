// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 fan tachometer
 *
 * The block counts tachometer pulses (pin "dc_fan_sensor") over a window
 * of TIMER_TV ticks of a 90 kHz timer and raises an interrupt when the
 * window ends.  There is no speed control in this block: on boards that
 * can control the fan this is done by a separate PWM output.
 *
 * Based on the vendor driver (drivers/soc/realtek/rtd129x/rtd129x_fan.c,
 * Copyright (c) 2017 Realtek Semiconductor Corp.).
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#define RTD1295_FAN_CTRL		0x00
#define  RTD1295_FAN_CTRL_EN		BIT(0)
#define  RTD1295_FAN_CTRL_INT_EN	BIT(1)
#define RTD1295_FAN_DEBOUNCE		0x04
#define  RTD1295_FAN_DEBOUNCE_W_EN	BIT(3)
#define RTD1295_FAN_TIMER_TV		0x08
#define RTD1295_FAN_TIMER_CV		0x0c
#define RTD1295_FAN_COUNTER_CV		0x10

#define RTD1295_FAN_TIMER_HZ		90000
/* 2 s measurement window: 15 rpm resolution with 2 pulses per revolution */
#define RTD1295_FAN_WINDOW		(2 * RTD1295_FAN_TIMER_HZ)
#define RTD1295_FAN_PULSES_PER_REV	2

struct rtd1295_fan {
	void __iomem *base;
	bool use_irq;
	u32 rpm;	/* last complete window, updated from the IRQ */
};

static u32 rtd1295_fan_rpm(u32 pulses, u32 ticks)
{
	if (!ticks)
		return 0;

	return div_u64((u64)pulses * RTD1295_FAN_TIMER_HZ * 60,
		       (u64)ticks * RTD1295_FAN_PULSES_PER_REV);
}

/* "Window done": the status is latched and acked in the MISC irq mux. */
static irqreturn_t rtd1295_fan_irq(int irq, void *data)
{
	struct rtd1295_fan *fan = data;
	u32 pulses;

	pulses = readl(fan->base + RTD1295_FAN_COUNTER_CV);
	WRITE_ONCE(fan->rpm, rtd1295_fan_rpm(pulses, RTD1295_FAN_WINDOW));

	return IRQ_HANDLED;
}

static umode_t rtd1295_fan_is_visible(const void *data,
				      enum hwmon_sensor_types type,
				      u32 attr, int channel)
{
	return 0444;
}

static int rtd1295_fan_read(struct device *dev, enum hwmon_sensor_types type,
			    u32 attr, int channel, long *val)
{
	struct rtd1295_fan *fan = dev_get_drvdata(dev);

	if (type != hwmon_fan || attr != hwmon_fan_input)
		return -EOPNOTSUPP;

	if (fan->use_irq) {
		*val = READ_ONCE(fan->rpm);
	} else {
		/* No IRQ: estimate from the running window */
		u32 pulses = readl(fan->base + RTD1295_FAN_COUNTER_CV);
		u32 ticks = readl(fan->base + RTD1295_FAN_TIMER_CV);

		*val = rtd1295_fan_rpm(pulses, ticks);
	}

	return 0;
}

static const struct hwmon_channel_info * const rtd1295_fan_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT),
	NULL
};

static const struct hwmon_ops rtd1295_fan_hwmon_ops = {
	.is_visible = rtd1295_fan_is_visible,
	.read = rtd1295_fan_read,
};

static const struct hwmon_chip_info rtd1295_fan_chip_info = {
	.ops = &rtd1295_fan_hwmon_ops,
	.info = rtd1295_fan_info,
};

static void rtd1295_fan_disable(void *data)
{
	struct rtd1295_fan *fan = data;

	writel(0, fan->base + RTD1295_FAN_CTRL);
}

static int rtd1295_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_control *rst;
	struct device *hwmon;
	struct rtd1295_fan *fan;
	struct clk *clk;
	int irq, ret;
	u32 ctrl;

	fan = devm_kzalloc(dev, sizeof(*fan), GFP_KERNEL);
	if (!fan)
		return -ENOMEM;

	fan->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(fan->base))
		return PTR_ERR(fan->base);

	clk = devm_clk_get_optional_enabled(dev, NULL);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "failed to get clock\n");

	rst = devm_reset_control_get_optional_exclusive_deasserted(dev, NULL);
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst), "failed to get reset\n");

	writel(0, fan->base + RTD1295_FAN_CTRL);
	writel(RTD1295_FAN_DEBOUNCE_W_EN | 0, fan->base + RTD1295_FAN_DEBOUNCE);
	writel(RTD1295_FAN_WINDOW, fan->base + RTD1295_FAN_TIMER_TV);

	ctrl = RTD1295_FAN_CTRL_EN;
	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = devm_request_irq(dev, irq, rtd1295_fan_irq, 0,
				       dev_name(dev), fan);
		if (ret)
			return dev_err_probe(dev, ret, "failed to request IRQ\n");
		fan->use_irq = true;
		ctrl |= RTD1295_FAN_CTRL_INT_EN;
	} else if (irq != -ENXIO) {
		/* -ENXIO: no interrupt described, poll the running window */
		return dev_err_probe(dev, irq ?: -EINVAL, "failed to get IRQ\n");
	}

	/* Registered after the IRQ so the block is stopped before free_irq() */
	ret = devm_add_action_or_reset(dev, rtd1295_fan_disable, fan);
	if (ret)
		return ret;

	writel(ctrl, fan->base + RTD1295_FAN_CTRL);

	hwmon = devm_hwmon_device_register_with_info(dev, "rtd1295_fan", fan,
						     &rtd1295_fan_chip_info,
						     NULL);
	return PTR_ERR_OR_ZERO(hwmon);
}

static const struct of_device_id rtd1295_fan_of_match[] = {
	{ .compatible = "realtek,rtd1295-fan-tach" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1295_fan_of_match);

static struct platform_driver rtd1295_fan_driver = {
	.probe = rtd1295_fan_probe,
	.driver = {
		.name = "rtd1295-fan",
		.of_match_table = rtd1295_fan_of_match,
	},
};
module_platform_driver(rtd1295_fan_driver);

MODULE_DESCRIPTION("Realtek RTD1295 fan tachometer driver");
MODULE_LICENSE("GPL");
