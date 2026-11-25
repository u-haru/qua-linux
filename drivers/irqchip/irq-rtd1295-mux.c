// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek RTD1295 ISO/MISC interrupt mux
 *
 * The ISO and MISC register blocks each collect up to 32 peripheral
 * interrupt sources into one GIC SPI (MISC: SPI 40, ISO: SPI 41).
 *
 * Each mux has two registers:
 *  - a status register (ISR): one latched bit per source, cleared by writing
 *    1 to that bit with bit 0 ("write_data") written as 0. Bit 0 is never a
 *    source, so we never write it.
 *  - an SCPU interrupt enable register: one enable bit per source. On MISC
 *    the enable bit is not always the same as the status bit (UART2 status
 *    bit 8 is enabled by bit 7, I2C3 status bit 23 by bit 28, ...), so each
 *    mux carries a status-bit to enable-bit table.
 *
 * hwirq numbers are status register bit numbers. Status bits that have no
 * enable bit are not routed (or not documented) and are refused.
 *
 * Based on the vendor irq-rtd129x.c (Copyright (C) 2017 Realtek).
 */

#include <linux/bits.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define RTD1295_MUX_NR_IRQS	32

struct rtd1295_mux_info {
	const char *name;
	/* enable register mask for each status bit, 0 = no such source */
	u32 isr_to_en[RTD1295_MUX_NR_IRQS];
};

struct rtd1295_mux {
	void __iomem *reg_isr;
	void __iomem *reg_en;
	const struct rtd1295_mux_info *info;
	struct irq_domain *domain;
	raw_spinlock_t lock;
	u32 valid;	/* status bits that have an enable bit */
};

static const struct rtd1295_mux_info rtd1295_misc_mux_info = {
	.name = "misc-irq-mux",
	.isr_to_en = {
		[3]  = BIT(3),	/* UART1 */
		[5]  = BIT(5),	/* UART1 timeout */
		[8]  = BIT(7),	/* UART2 */
		[10] = BIT(10),	/* RTC minute */
		[11] = BIT(11),	/* RTC hour */
		[12] = BIT(12),	/* RTC date */
		[13] = BIT(6),	/* UART2 timeout */
		[14] = BIT(14),	/* I2C5 */
		[15] = BIT(15),	/* I2C4 */
		[19] = BIT(19),	/* GPIO assert */
		[20] = BIT(20),	/* GPIO deassert */
		[21] = BIT(21),	/* LSADC0 */
		[22] = BIT(22),	/* LSADC1 */
		[23] = BIT(28),	/* I2C3 */
		[24] = BIT(24),	/* smart card 0 */
		[26] = BIT(26),	/* I2C2 */
		[27] = BIT(27),	/* GSPI */
		[29] = BIT(29),	/* fan */
	},
};

static const struct rtd1295_mux_info rtd1295_iso_mux_info = {
	.name = "iso-irq-mux",
	.isr_to_en = {
		[2]  = BIT(2),	/* UART0 */
		[5]  = BIT(5),	/* IrDA */
		[8]  = BIT(8),	/* I2C0 */
		[11] = BIT(11),	/* I2C1 */
		[12] = BIT(12),	/* RTC half second */
		[13] = BIT(13),	/* RTC alarm */
		[19] = BIT(19),	/* GPIO assert */
		[20] = BIT(20),	/* GPIO deassert */
		[29] = BIT(29),	/* GPHY dv */
		[30] = BIT(30),	/* GPHY av */
		[31] = BIT(31),	/* I2C1 request (HDMI SCDC) */
	},
};

static void rtd1295_mux_ack_irq(struct irq_data *d)
{
	struct rtd1295_mux *mux = irq_data_get_irq_chip_data(d);

	writel_relaxed(BIT(irqd_to_hwirq(d)), mux->reg_isr);
}

static void rtd1295_mux_update_en(struct irq_data *d, bool enable)
{
	struct rtd1295_mux *mux = irq_data_get_irq_chip_data(d);
	u32 bit = mux->info->isr_to_en[irqd_to_hwirq(d)];
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&mux->lock, flags);
	val = readl_relaxed(mux->reg_en);
	if (enable)
		val |= bit;
	else
		val &= ~bit;
	writel_relaxed(val, mux->reg_en);
	raw_spin_unlock_irqrestore(&mux->lock, flags);
}

static void rtd1295_mux_mask_irq(struct irq_data *d)
{
	rtd1295_mux_update_en(d, false);
}

static void rtd1295_mux_unmask_irq(struct irq_data *d)
{
	rtd1295_mux_update_en(d, true);
}

static struct irq_chip rtd1295_mux_irq_chip = {
	.name		= "rtd1295-mux",
	.irq_ack	= rtd1295_mux_ack_irq,
	.irq_mask	= rtd1295_mux_mask_irq,
	.irq_unmask	= rtd1295_mux_unmask_irq,
	.flags		= IRQCHIP_SKIP_SET_WAKE,
};

static void rtd1295_mux_handle_irq(struct irq_desc *desc)
{
	struct rtd1295_mux *mux = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	u32 status, enable;
	unsigned long pending;
	int i;

	chained_irq_enter(chip, desc);

	status = readl_relaxed(mux->reg_isr) & mux->valid;
	enable = readl_relaxed(mux->reg_en);

	pending = 0;
	for (i = 0; i < RTD1295_MUX_NR_IRQS; i++)
		if ((status & BIT(i)) && (enable & mux->info->isr_to_en[i]))
			pending |= BIT(i);

	for_each_set_bit(i, &pending, RTD1295_MUX_NR_IRQS)
		generic_handle_domain_irq(mux->domain, i);

	chained_irq_exit(chip, desc);
}

static int rtd1295_mux_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	struct rtd1295_mux *mux = d->host_data;

	if (hw >= RTD1295_MUX_NR_IRQS || !(mux->valid & BIT(hw)))
		return -EINVAL;

	irq_set_chip_data(irq, mux);
	irq_set_chip_and_handler(irq, &rtd1295_mux_irq_chip, handle_level_irq);
	irq_set_probe(irq);

	return 0;
}

static const struct irq_domain_ops rtd1295_mux_domain_ops = {
	.xlate	= irq_domain_xlate_onecell,
	.map	= rtd1295_mux_map,
};

static void __iomem *rtd1295_mux_ioremap(struct platform_device *pdev, unsigned int index)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (!res)
		return IOMEM_ERR_PTR(-EINVAL);

	/*
	 * Not requested: both registers sit inside the ISO/MISC syscon and
	 * inside the GPIO interrupt status window that gpio-rtd claims.
	 */
	return devm_ioremap(&pdev->dev, res->start, resource_size(res)) ?:
	       IOMEM_ERR_PTR(-ENOMEM);
}

static int rtd1295_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtd1295_mux *mux;
	int i, irq;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return -ENOMEM;

	mux->info = of_device_get_match_data(dev);
	if (!mux->info)
		return -EINVAL;

	raw_spin_lock_init(&mux->lock);

	for (i = 0; i < RTD1295_MUX_NR_IRQS; i++)
		if (mux->info->isr_to_en[i])
			mux->valid |= BIT(i);

	mux->reg_isr = rtd1295_mux_ioremap(pdev, 0);
	if (IS_ERR(mux->reg_isr))
		return dev_err_probe(dev, PTR_ERR(mux->reg_isr), "status register\n");

	mux->reg_en = rtd1295_mux_ioremap(pdev, 1);
	if (IS_ERR(mux->reg_en))
		return dev_err_probe(dev, PTR_ERR(mux->reg_en), "enable register\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * Start with every source disabled and the stale status of the known
	 * ones cleared; the bootloader may have left the UART0 enable on.
	 */
	writel_relaxed(0, mux->reg_en);
	writel_relaxed(mux->valid, mux->reg_isr);

	mux->domain = irq_domain_create_linear(dev_fwnode(dev), RTD1295_MUX_NR_IRQS,
					       &rtd1295_mux_domain_ops, mux);
	if (!mux->domain)
		return -ENOMEM;

	irq_set_chained_handler_and_data(irq, rtd1295_mux_handle_irq, mux);

	return 0;
}

static const struct of_device_id rtd1295_mux_of_match[] = {
	{ .compatible = "realtek,rtd1295-misc-irq-mux", .data = &rtd1295_misc_mux_info },
	{ .compatible = "realtek,rtd1295-iso-irq-mux", .data = &rtd1295_iso_mux_info },
	{ }
};

static struct platform_driver rtd1295_mux_driver = {
	.probe = rtd1295_mux_probe,
	.driver = {
		.name = "rtd1295-irq-mux",
		.of_match_table = rtd1295_mux_of_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(rtd1295_mux_driver);
