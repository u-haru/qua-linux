// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1293/RTD1295/RTD1296 clock controllers
 *
 * Two providers, each a child of the syscon it lives in:
 *  - CRT (0x98000000): clock gates at 0x0c, 0x10 and 0x450, plus read-only
 *    rates of the BUS, DCSB and DDS PLLs and the system bus muxes, and the
 *    SCPU clock (PLL + divider), the only one reprogrammed (cpufreq).
 *  - ISO (0x98007000): clock gates at 0x8c.
 *
 * Register layout taken from the Realtek 4.9 BSP (cc-rtd129x.c, cgc.c and
 * the clock-gate-controller nodes of rtd-1295.dtsi). The other PLLs are never
 * reprogrammed: they are set up by the boot loader and shared with the
 * audio/video firmware.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk-provider.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/realtek,rtd1295-clk.h>

#define RTD1295_CRT_NR_CLKS	(RTD1295_CRT_PLL_DDSB + 1)
#define RTD1295_ISO_NR_CLKS	(RTD1295_ISO_CLK_EN_ETN_SYS + 1)

/* CRT registers */
#define CRT_CLOCK_ENABLE1	0x0c
#define CRT_CLOCK_ENABLE2	0x10
#define CRT_SYS_CLKSEL_SB2	0x18
#define CRT_SYS_CLKSEL		0x30
#define  CRT_SYS_CLKSEL_SYS	BIT(0)
#define  CRT_SYS_CLKSEL_SCPU	GENMASK(8, 7)
#define CRT_CLOCK_ENABLE3	0x450
#define CRT_PLL_SCPU1		0x500
#define  PLL_SCPU_OC_MASK	GENMASK(2, 0)	/* 4: OC off, 5: OC on (BSP) */
#define CRT_PLL_SCPU2		0x504
#define CRT_PLL_SCPU_STS	0x51c
#define  PLL_SCPU_OC_DONE	BIT(20)
#define CRT_PLL_BUS2		0x524
#define CRT_PLL_DCSB2		0x544
#define CRT_PLL_DDSA2		0x564
#define CRT_PLL_DDSB2		0x584

/* NF type PLL: rate = parent * (N + 3 + F / 2048) */
#define PLL_NF_N		GENMASK(18, 11)
#define PLL_NF_F		GENMASK(10, 0)

/* ISO registers */
#define ISO_CLOCK_ENABLE	0x8c

/* Gate parents */
#define P_NONE			0
#define P_OSC			1

struct rtd_gate_desc {
	unsigned int id;
	const char *name;
	u16 reg;
	u8 bit;
	u8 parent;
	unsigned long flags;
};

struct rtd_clk_data {
	const struct rtd_gate_desc *gates;
	unsigned int num_gates;
	unsigned int num_clks;
	bool has_plls;
};

struct rtd_clk_gate {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct rtd_clk_reg {
	struct clk_hw hw;
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

#define to_rtd_gate(_hw) container_of(_hw, struct rtd_clk_gate, hw)
#define to_rtd_reg(_hw) container_of(_hw, struct rtd_clk_reg, hw)

static const struct clk_parent_data rtd_osc_parent = {
	.index = 0,
	.name = "osc27M",
};

/*
 * Gates marked CLK_IS_CRITICAL feed blocks that Linux does not own (or that
 * the console and register accesses of other drivers depend on) and must
 * never be turned off. Gates not listed at all (SCPU, ACPU, VO, AIO, GPU,
 * VE, DCU, RSA, ...) are not exposed and therefore never touched.
 */
#define GATE(_id, _name, _reg, _bit, _parent, _flags) \
	{ .id = _id, .name = _name, .reg = _reg, .bit = _bit, \
	  .parent = _parent, .flags = _flags }

static const struct rtd_gate_desc rtd1295_crt_gates[] = {
	/* MISC (SB2/MISC register blocks, GPIO, UART1/2, timers, irq mux) */
	GATE(RTD1295_CRT_CLK_EN_MISC, "clk_en_misc", CRT_CLOCK_ENABLE1, 0, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_PCIE0, "clk_en_pcie0", CRT_CLOCK_ENABLE1, 1, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SATA_0, "clk_en_sata_0", CRT_CLOCK_ENABLE1, 2, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_GSPI, "clk_en_gspi", CRT_CLOCK_ENABLE1, 3, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_USB, "clk_en_usb", CRT_CLOCK_ENABLE1, 4, P_NONE, 0),
	/* ISO block bus access (UART0, watchdog, irq mux, ...) */
	GATE(RTD1295_CRT_CLK_EN_ISO_MISC, "clk_en_iso_misc", CRT_CLOCK_ENABLE1, 6, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_SATA_ALIVE_0, "clk_en_sata_alive_0", CRT_CLOCK_ENABLE1, 7, P_NONE, 0),
	/* HDMI is driven by the audio firmware on the ACPU */
	GATE(RTD1295_CRT_CLK_EN_HDMI, "clk_en_hdmi", CRT_CLOCK_ENABLE1, 8, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_ETN, "clk_en_etn", CRT_CLOCK_ENABLE1, 9, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_LVDS, "clk_en_lvds", CRT_CLOCK_ENABLE1, 16, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SE, "clk_en_se", CRT_CLOCK_ENABLE1, 17, P_NONE, 0),
	/* CP (crypto, used by OP-TEE), MD (move data), TP: used by firmware */
	GATE(RTD1295_CRT_CLK_EN_CP, "clk_en_cp", CRT_CLOCK_ENABLE1, 19, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_MD, "clk_en_md", CRT_CLOCK_ENABLE1, 20, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_TP, "clk_en_tp", CRT_CLOCK_ENABLE1, 21, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_NF, "clk_en_nf", CRT_CLOCK_ENABLE1, 23, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_EMMC, "clk_en_emmc", CRT_CLOCK_ENABLE1, 24, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_CR, "clk_en_cr", CRT_CLOCK_ENABLE1, 25, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SDIO_IP, "clk_en_sdio_ip", CRT_CLOCK_ENABLE1, 26, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_MIPI, "clk_en_mipi", CRT_CLOCK_ENABLE1, 27, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_EMMC_IP, "clk_en_emmc_ip", CRT_CLOCK_ENABLE1, 28, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SDIO, "clk_en_sdio", CRT_CLOCK_ENABLE1, 30, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SD_IP, "clk_en_sd_ip", CRT_CLOCK_ENABLE1, 31, P_NONE, 0),

	GATE(RTD1295_CRT_CLK_EN_NAT, "clk_en_nat", CRT_CLOCK_ENABLE2, 0, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_I2C_5, "clk_en_misc_i2c_5", CRT_CLOCK_ENABLE2, 1, P_OSC, 0),
	GATE(RTD1295_CRT_CLK_EN_JPEG, "clk_en_jpeg", CRT_CLOCK_ENABLE2, 3, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_PCIE1, "clk_en_pcie1", CRT_CLOCK_ENABLE2, 5, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_SC, "clk_en_misc_sc", CRT_CLOCK_ENABLE2, 6, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_CBUS_TX, "clk_en_cbus_tx", CRT_CLOCK_ENABLE2, 7, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_RTC, "clk_en_misc_rtc", CRT_CLOCK_ENABLE2, 10, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_I2C_4, "clk_en_misc_i2c_4", CRT_CLOCK_ENABLE2, 13, P_OSC, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_I2C_3, "clk_en_misc_i2c_3", CRT_CLOCK_ENABLE2, 14, P_OSC, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_I2C_2, "clk_en_misc_i2c_2", CRT_CLOCK_ENABLE2, 15, P_OSC, 0),
	GATE(RTD1295_CRT_CLK_EN_MISC_I2C_1, "clk_en_misc_i2c_1", CRT_CLOCK_ENABLE2, 16, P_OSC, 0),
	GATE(RTD1295_CRT_CLK_EN_HDMIRX, "clk_en_hdmirx", CRT_CLOCK_ENABLE2, 24, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SATA_1, "clk_en_sata_1", CRT_CLOCK_ENABLE2, 25, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_SATA_ALIVE_1, "clk_en_sata_alive_1", CRT_CLOCK_ENABLE2, 26, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_UR2, "clk_en_ur2", CRT_CLOCK_ENABLE2, 27, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_UR1, "clk_en_ur1", CRT_CLOCK_ENABLE2, 28, P_NONE, 0),
	GATE(RTD1295_CRT_CLK_EN_FAN, "clk_en_fan", CRT_CLOCK_ENABLE2, 29, P_NONE, 0),
	/* DDR PHY */
	GATE(RTD1295_CRT_CLK_EN_DCPHY_0, "clk_en_dcphy_0", CRT_CLOCK_ENABLE2, 30, P_NONE, CLK_IS_CRITICAL),
	GATE(RTD1295_CRT_CLK_EN_DCPHY_1, "clk_en_dcphy_1", CRT_CLOCK_ENABLE2, 31, P_NONE, CLK_IS_CRITICAL),

	GATE(RTD1295_CRT_CLK_EN_LSADC, "clk_en_lsadc", CRT_CLOCK_ENABLE3, 2, P_NONE, 0),
};

static const struct rtd_gate_desc rtd1295_iso_gates[] = {
	GATE(RTD1295_ISO_CLK_EN_MISC_CEC0, "clk_en_misc_cec0", ISO_CLOCK_ENABLE, 2, P_NONE, 0),
	GATE(RTD1295_ISO_CLK_EN_CBUSRX_SYS, "clk_en_cbusrx_sys", ISO_CLOCK_ENABLE, 3, P_NONE, 0),
	GATE(RTD1295_ISO_CLK_EN_CBUSTX_SYS, "clk_en_cbustx_sys", ISO_CLOCK_ENABLE, 4, P_NONE, 0),
	GATE(RTD1295_ISO_CLK_EN_CBUS_SYS, "clk_en_cbus_sys", ISO_CLOCK_ENABLE, 5, P_NONE, 0),
	GATE(RTD1295_ISO_CLK_EN_CBUS_OSC, "clk_en_cbus_osc", ISO_CLOCK_ENABLE, 6, P_OSC, 0),
	GATE(RTD1295_ISO_CLK_EN_MISC_IR, "clk_en_misc_ir", ISO_CLOCK_ENABLE, 7, P_NONE, 0),
	/* UART0 is the console and is not given a clock in the DT */
	GATE(RTD1295_ISO_CLK_EN_MISC_UR0, "clk_en_misc_ur0", ISO_CLOCK_ENABLE, 8, P_OSC, CLK_IS_CRITICAL),
	GATE(RTD1295_ISO_CLK_EN_I2C0, "clk_en_i2c0", ISO_CLOCK_ENABLE, 9, P_OSC, 0),
	GATE(RTD1295_ISO_CLK_EN_I2C1, "clk_en_i2c1", ISO_CLOCK_ENABLE, 10, P_OSC, 0),
	/* Kept on by the BSP as well ("ignore-unused-clocks") */
	GATE(RTD1295_ISO_CLK_EN_ETN_250M, "clk_en_etn_250m", ISO_CLOCK_ENABLE, 11, P_NONE, CLK_IGNORE_UNUSED),
	GATE(RTD1295_ISO_CLK_EN_ETN_SYS, "clk_en_etn_sys", ISO_CLOCK_ENABLE, 12, P_NONE, CLK_IGNORE_UNUSED),
};

static int rtd_gate_enable(struct clk_hw *hw)
{
	struct rtd_clk_gate *gate = to_rtd_gate(hw);

	return regmap_set_bits(gate->regmap, gate->reg, gate->mask);
}

static void rtd_gate_disable(struct clk_hw *hw)
{
	struct rtd_clk_gate *gate = to_rtd_gate(hw);

	regmap_clear_bits(gate->regmap, gate->reg, gate->mask);
}

static int rtd_gate_is_enabled(struct clk_hw *hw)
{
	struct rtd_clk_gate *gate = to_rtd_gate(hw);

	return regmap_test_bits(gate->regmap, gate->reg, gate->mask) > 0;
}

static const struct clk_ops rtd_gate_ops = {
	.enable = rtd_gate_enable,
	.disable = rtd_gate_disable,
	.is_enabled = rtd_gate_is_enabled,
};

static unsigned long rtd_pll_nf_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct rtd_clk_reg *pll = to_rtd_reg(hw);
	u32 val, n, f;

	if (regmap_read(pll->regmap, pll->reg, &val))
		return 0;

	n = FIELD_GET(PLL_NF_N, val);
	f = FIELD_GET(PLL_NF_F, val);

	return parent_rate * (n + 3) + DIV_ROUND_CLOSEST_ULL((u64)parent_rate * f, 2048);
}

static const struct clk_ops rtd_pll_nf_ro_ops = {
	.recalc_rate = rtd_pll_nf_recalc_rate,
};

/* SCPU divider: 0 and 1 -> /1, 2 -> /2, 3 -> /4 */
static unsigned long rtd_scpu_div_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct rtd_clk_reg *div = to_rtd_reg(hw);
	u32 val;

	if (regmap_read(div->regmap, div->reg, &val))
		return 0;

	switch (FIELD_GET(CRT_SYS_CLKSEL_SCPU, val)) {
	case 2:
		return parent_rate / 2;
	case 3:
		return parent_rate / 4;
	default:
		return parent_rate;
	}
}

/*
 * The SCPU clock is the only one Linux reprograms (for cpufreq). The PLL
 * slews on the fly ("OC"); the BSP never bypasses it. Rates and the divider
 * glitch workaround come from the 4.9 BSP (cc-rtd129x.c / clk-pll.c).
 */
struct rtd_scpu_rate {
	unsigned long rate;
	u8 n;
	u16 f;
	u8 sel;		/* SYS_CLKSEL[8:7]: 1 = /1, 2 = /2, 3 = /4 */
};

static const struct rtd_scpu_rate rtd_scpu_rates[] = {
	{  300000000, 41,  910, 3 }, {  400000000, 56,  531, 3 },
	{  500000000, 34,   75, 2 }, {  600000000, 41,  910, 2 },
	{  700000000, 48, 1745, 2 }, {  800000000, 56,  531, 2 },
	{  900000000, 63, 1365, 2 }, { 1000000000, 34,   75, 1 },
	{ 1100000000, 37, 1517, 1 }, { 1200000000, 41,  910, 1 },
	{ 1300000000, 45,  303, 1 }, { 1400000000, 48, 1745, 1 },
};

/*
 * Wait for the PLL slew ("OC done"). This is called from .set_rate, which
 * runs in process context under the clk prepare_lock (that serialises all
 * set_rate calls, so no driver lock is needed), so sleep while polling
 * instead of spinning with interrupts off. 2 ms is the BSP limit.
 */
#define RTD_SCPU_OC_POLL_US	10
#define RTD_SCPU_OC_TIMEOUT_US	2000

static int rtd_scpu_write_pll(struct regmap *m, u8 n, u16 f)
{
	u32 v;

	regmap_update_bits(m, CRT_PLL_SCPU1, PLL_SCPU_OC_MASK, 0x4);
	regmap_update_bits(m, CRT_PLL_SCPU2, PLL_NF_N | PLL_NF_F,
			   FIELD_PREP(PLL_NF_N, n) | FIELD_PREP(PLL_NF_F, f));
	regmap_update_bits(m, CRT_PLL_SCPU1, PLL_SCPU_OC_MASK, 0x5);
	return regmap_read_poll_timeout(m, CRT_PLL_SCPU_STS, v,
					v & PLL_SCPU_OC_DONE,
					RTD_SCPU_OC_POLL_US,
					RTD_SCPU_OC_TIMEOUT_US);
}

static void rtd_scpu_write_sel(struct regmap *m, u8 sel)
{
	regmap_update_bits(m, CRT_SYS_CLKSEL, CRT_SYS_CLKSEL_SCPU,
			   FIELD_PREP(CRT_SYS_CLKSEL_SCPU, sel));
}

/*
 * Highest table rate <= req->rate within [min_rate, max_rate]; if the request
 * is below every allowed rate, the lowest allowed one (table is ascending).
 */
static int rtd_scpu_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	unsigned long best = 0, lowest = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(rtd_scpu_rates); i++) {
		unsigned long r = rtd_scpu_rates[i].rate;

		if (r < req->min_rate || r > req->max_rate)
			continue;
		if (!lowest)
			lowest = r;
		if (r <= req->rate)
			best = r;
	}

	if (!best)
		best = lowest;
	if (!best)
		return -EINVAL;

	req->rate = best;
	return 0;
}

static unsigned long rtd_scpu_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct rtd_clk_reg *c = to_rtd_reg(hw);
	u32 pll, sel;
	int i;

	if (!regmap_read(c->regmap, CRT_PLL_SCPU2, &pll) &&
	    !regmap_read(c->regmap, CRT_SYS_CLKSEL, &sel)) {
		sel = FIELD_GET(CRT_SYS_CLKSEL_SCPU, sel) ?: 1;
		for (i = 0; i < ARRAY_SIZE(rtd_scpu_rates); i++)
			if (rtd_scpu_rates[i].n == FIELD_GET(PLL_NF_N, pll) &&
			    rtd_scpu_rates[i].f == FIELD_GET(PLL_NF_F, pll) &&
			    rtd_scpu_rates[i].sel == sel)
				return rtd_scpu_rates[i].rate;
	}

	return rtd_scpu_div_recalc_rate(hw, parent_rate);
}

static int rtd_scpu_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct rtd_clk_reg *c = to_rtd_reg(hw);
	const struct rtd_scpu_rate *t = NULL;
	u32 v;
	u8 cur;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(rtd_scpu_rates); i++)
		if (rtd_scpu_rates[i].rate == rate)
			t = &rtd_scpu_rates[i];
	if (!t)
		return -EINVAL;

	regmap_read(c->regmap, CRT_SYS_CLKSEL, &v);
	cur = FIELD_GET(CRT_SYS_CLKSEL_SCPU, v) ?: 1;
	/* BSP workaround: change to or from /1 only with the PLL at 1 GHz */
	if (t->sel != cur && (t->sel == 1 || cur == 1)) {
		ret = rtd_scpu_write_pll(c->regmap, 34, 75);
		if (ret)
			return ret;
		rtd_scpu_write_sel(c->regmap, t->sel);
		cur = t->sel;
	}
	if (t->sel > cur)
		rtd_scpu_write_sel(c->regmap, t->sel);
	ret = rtd_scpu_write_pll(c->regmap, t->n, t->f);
	if (!ret && t->sel < cur)
		rtd_scpu_write_sel(c->regmap, t->sel);
	return ret;
}

static const struct clk_ops rtd_scpu_ops = {
	.recalc_rate	= rtd_scpu_recalc_rate,
	.determine_rate	= rtd_scpu_determine_rate,
	.set_rate	= rtd_scpu_set_rate,
};

static u8 rtd_mux_get_parent(struct clk_hw *hw)
{
	struct rtd_clk_reg *mux = to_rtd_reg(hw);
	u32 val;

	if (regmap_read(mux->regmap, mux->reg, &val))
		return 0;

	return !!(val & mux->mask);
}

static const struct clk_ops rtd_mux_ro_ops = {
	.get_parent = rtd_mux_get_parent,
};

static struct clk_hw *rtd_register_reg_clk(struct device *dev,
					   struct regmap *regmap,
					   const char *name, u32 reg, u32 mask,
					   const struct clk_ops *ops,
					   const struct clk_parent_data *pdata,
					   unsigned int num_parents)
{
	struct clk_init_data init = {};
	struct rtd_clk_reg *clk;
	int ret;

	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->regmap = regmap;
	clk->reg = reg;
	clk->mask = mask;

	init.name = name;
	init.ops = ops;
	init.parent_data = pdata;
	init.num_parents = num_parents;
	init.flags = CLK_GET_RATE_NOCACHE;
	clk->hw.init = &init;

	ret = devm_clk_hw_register(dev, &clk->hw);
	if (ret)
		return ERR_PTR(ret);

	return &clk->hw;
}

static int rtd1295_register_plls(struct device *dev, struct regmap *regmap,
				 struct clk_hw **hws)
{
	static const struct {
		unsigned int id;
		const char *name;
		u32 reg;
	} plls[] = {
		{ RTD1295_CRT_PLL_SCPU, "pll_scpu", CRT_PLL_SCPU2 },
		{ RTD1295_CRT_PLL_BUS, "pll_bus", CRT_PLL_BUS2 },
		{ RTD1295_CRT_PLL_DCSB, "pll_dcsb", CRT_PLL_DCSB2 },
		{ RTD1295_CRT_PLL_DDSA, "pll_ddsa", CRT_PLL_DDSA2 },
		{ RTD1295_CRT_PLL_DDSB, "pll_ddsb", CRT_PLL_DDSB2 },
	};
	struct clk_parent_data pdata[2] = {};
	struct clk_hw *hw;
	int i;

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		hw = rtd_register_reg_clk(dev, regmap, plls[i].name, plls[i].reg,
					  0, &rtd_pll_nf_ro_ops,
					  &rtd_osc_parent, 1);
		if (IS_ERR(hw))
			return PTR_ERR(hw);
		hws[plls[i].id] = hw;
	}

	pdata[0].hw = hws[RTD1295_CRT_PLL_SCPU];
	hw = rtd_register_reg_clk(dev, regmap, "clk_scpu", CRT_SYS_CLKSEL,
				  CRT_SYS_CLKSEL_SCPU, &rtd_scpu_ops,
				  pdata, 1);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[RTD1295_CRT_CLK_SCPU] = hw;

	hw = devm_clk_hw_register_fixed_factor_parent_hw(dev, "pll_bus_div2",
							 hws[RTD1295_CRT_PLL_BUS],
							 0, 1, 2);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[RTD1295_CRT_PLL_BUS_DIV2] = hw;

	pdata[0].hw = hws[RTD1295_CRT_PLL_BUS];
	pdata[1].hw = hws[RTD1295_CRT_PLL_BUS_DIV2];
	hw = rtd_register_reg_clk(dev, regmap, "clk_sys", CRT_SYS_CLKSEL,
				  CRT_SYS_CLKSEL_SYS, &rtd_mux_ro_ops, pdata, 2);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[RTD1295_CRT_CLK_SYS] = hw;

	pdata[0].hw = hws[RTD1295_CRT_CLK_SYS];
	pdata[1] = rtd_osc_parent;
	hw = rtd_register_reg_clk(dev, regmap, "clk_sys_sb2", CRT_SYS_CLKSEL_SB2,
				  BIT(0), &rtd_mux_ro_ops, pdata, 2);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	hws[RTD1295_CRT_CLK_SYS_SB2] = hw;

	return 0;
}

static int rtd1295_register_gates(struct device *dev, struct regmap *regmap,
				  const struct rtd_clk_data *data,
				  struct clk_hw **hws)
{
	unsigned int i;
	int ret;

	for (i = 0; i < data->num_gates; i++) {
		const struct rtd_gate_desc *desc = &data->gates[i];
		struct clk_init_data init = {};
		struct rtd_clk_gate *gate;

		gate = devm_kzalloc(dev, sizeof(*gate), GFP_KERNEL);
		if (!gate)
			return -ENOMEM;

		gate->regmap = regmap;
		gate->reg = desc->reg;
		gate->mask = BIT(desc->bit);

		init.name = desc->name;
		init.ops = &rtd_gate_ops;
		init.flags = desc->flags;
		if (desc->parent == P_OSC) {
			init.parent_data = &rtd_osc_parent;
			init.num_parents = 1;
		}
		gate->hw.init = &init;

		ret = devm_clk_hw_register(dev, &gate->hw);
		if (ret)
			return dev_err_probe(dev, ret, "failed to register %s\n",
					     desc->name);
		hws[desc->id] = &gate->hw;
	}

	return 0;
}

static int rtd1295_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct rtd_clk_data *data;
	struct clk_hw_onecell_data *clk_data;
	struct device_node *parent;
	struct regmap *regmap;
	unsigned int i;
	int ret;

	data = of_device_get_match_data(dev);
	if (!data)
		return -EINVAL;

	parent = of_get_parent(dev->of_node);
	regmap = syscon_node_to_regmap(parent);
	of_node_put(parent);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap),
				     "failed to get parent syscon regmap\n");

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws, data->num_clks),
				GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;
	clk_data->num = data->num_clks;
	for (i = 0; i < data->num_clks; i++)
		clk_data->hws[i] = ERR_PTR(-ENOENT);

	if (data->has_plls) {
		ret = rtd1295_register_plls(dev, regmap, clk_data->hws);
		if (ret)
			return dev_err_probe(dev, ret, "failed to register PLLs\n");
	}

	ret = rtd1295_register_gates(dev, regmap, data, clk_data->hws);
	if (ret)
		return ret;

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, clk_data);
}

static const struct rtd_clk_data rtd1295_crt_data = {
	.gates = rtd1295_crt_gates,
	.num_gates = ARRAY_SIZE(rtd1295_crt_gates),
	.num_clks = RTD1295_CRT_NR_CLKS,
	.has_plls = true,
};

static const struct rtd_clk_data rtd1295_iso_data = {
	.gates = rtd1295_iso_gates,
	.num_gates = ARRAY_SIZE(rtd1295_iso_gates),
	.num_clks = RTD1295_ISO_NR_CLKS,
};

static const struct of_device_id rtd1295_clk_of_match[] = {
	{ .compatible = "realtek,rtd1295-crt-clk", .data = &rtd1295_crt_data },
	{ .compatible = "realtek,rtd1295-iso-clk", .data = &rtd1295_iso_data },
	{ /* sentinel */ }
};

static struct platform_driver rtd1295_clk_driver = {
	.probe = rtd1295_clk_probe,
	.driver = {
		.name = "rtd1295-clk",
		.of_match_table = rtd1295_clk_of_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(rtd1295_clk_driver);
