// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek RTD1295 PCIe host controller driver
 *
 * The RTD1295 has two single-lane root complexes: a PCIe 1.1 controller
 * ("slot 1", registers at 0x9804e000) and a PCIe 2.0 controller ("slot 2",
 * registers at 0x9803b000). Neither exposes its own root port in config
 * space: only device 0, function 0 of the link partner is reachable, through
 * an indirect configuration access engine, and it appears directly on the
 * root bus. Legacy INTx is wired straight to a GIC SPI; there is no MSI.
 *
 * Based on the Realtek vendor kernel (pcie-rtd129x-slot{1,2}.c),
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/pci-rtd1295.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/soc/realtek/rtd1295-rbus.h>
#include <linux/spinlock.h>

#include "../pci.h"

/* Root complex DBI (its own type 1 header lives at 0x000-0xbff) */
#define RTD_PCIE_RC_COMMAND		0x004
#define RTD_PCIE_RC_MEM_BASE_LIMIT	0x020
#define RTD_PCIE_RC_PREF_BASE_LIMIT	0x024
#define RTD_PCIE_RC_LNKCTL2		0x0a0
#define  RTD_PCIE_RC_LNKCTL2_TLS	GENMASK(3, 0)
#define RTD_PCIE_LINK_INIT		0x710

/* Realtek wrapper registers */
#define RTD_PCIE_SYS_CTR		0xc00
#define RTD_PCIE_INDIR_CTR		0xc14
#define RTD_PCIE_MDIO_CTR		0xc1c
#define RTD_PCIE_CFG_CT			0xc38
#define  RTD_PCIE_CFG_CT_GO		BIT(0)
#define RTD_PCIE_CFG_EN			0xc3c
#define  RTD_PCIE_CFG_EN_BYTE_CNT	GENMASK(7, 4)
#define  RTD_PCIE_CFG_EN_BYTE_EN	BIT(1)
#define  RTD_PCIE_CFG_EN_WRITE		BIT(0)
#define RTD_PCIE_CFG_ST			0xc40
#define  RTD_PCIE_CFG_ST_ERROR		BIT(1)
#define  RTD_PCIE_CFG_ST_DONE		BIT(0)
#define RTD_PCIE_CFG_ADDR		0xc44
#define RTD_PCIE_CFG_WDATA		0xc48
#define RTD_PCIE_CFG_RDATA		0xc4c
#define RTD_PCIE_DIR_ST			0xc74
#define  RTD_PCIE_DIR_ST_TIMEOUT	BIT(0)
#define RTD_PCIE_DIR_EN			0xc78
#define RTD_PCIE_DLLP_ERR		0xc7c
#define  RTD_PCIE_DLLP_ERR_MASK		GENMASK(4, 0)
#define RTD_PCIE_MAC_ST			0xcb4
#define  RTD_PCIE_MAC_ST_LINK		BIT(11)
#define RTD_PCIE_TRANS_BASE		0xcfc
#define RTD_PCIE_TRANS_MASK		0xd00
#define RTD_PCIE_TRANS_ADDR		0xd04

/* Values written by the vendor driver */
#define RTD_PCIE_SYS_CTR_MDIO		0x00140010
#define RTD_PCIE_SYS_CTR_RUN		0x001e0022
#define RTD_PCIE_INDIR_CTR_WRITE	0x12
#define RTD_PCIE_INDIR_CTR_READ		0x10
#define RTD_PCIE_LINK_INIT_VAL		0x00010120
#define RTD_PCIE_TRANS_MASK_4K		0xfffff000
#define RTD_PCIE_TRANS_MASK_2K		0xfffff800
#define RTD_PCIE_MMIO_DLLP_RETRIES	5

/* Indirect config address: bus[31:24] dev[23:19] fn[18:16] reg[11:0] */
#define RTD_PCIE_CFG_ADDRESS(bus, devfn, where) \
	(((bus) << 24) | (PCI_SLOT(devfn) << 19) | \
	 (PCI_FUNC(devfn) << 16) | (where))

/*
 * The engine completes in microseconds; these bound how long a dead link can
 * keep IRQs off (the PCI core holds pci_lock with irqsave around ->read and
 * ->write). The vendor driver polls for up to 1 s.
 */
#define RTD_PCIE_CFG_POLL_US		10
#define RTD_PCIE_CFG_TIMEOUT_US		(10 * USEC_PER_MSEC)
#define RTD_PCIE_CFG_DLLP_RETRIES	5
#define RTD_PCIE_LINK_TIMEOUT_US	(60 * USEC_PER_MSEC)

/* SB2 chip revision, read through the &sb2 syscon */
#define RTD_SB2_CHIP_REV		0x204
#define RTD_CHIP_REV_A00		0x00000
#define RTD_CHIP_REV_A01		0x10000

/* "phy-mux" block (0x9801c600), only touched on A00 silicon */
#define RTD_PHY_MUX_SEL			0x00
#define RTD_PHY_MUX_CFG8		0x08
#define RTD_PHY_MUX_CFG14		0x14
#define  RTD_PHY_MUX_CFG14_MASK		GENMASK(2, 0)


static const char * const rtd1295_pcie_reset_names[] = {
	/* deassertion order used by the vendor driver */
	"stitch", "rstn", "core", "power", "nonstitch", "phy", "phy_mdio",
};

struct rtd1295_pcie_data {
	const u32 *phy_a;		/* MDIO writes for A00/A01 silicon */
	unsigned int phy_a_len;
	const u32 *phy_b;		/* MDIO writes for B00 and later */
	unsigned int phy_b_len;
	u32 dir_en;			/* value for RTD_PCIE_DIR_EN */
	u32 phy_mux_sel_mask;		/* A00 PHY mux field to clear */
	bool c68_quirk;			/* offset 0xc68 also needs the 2K page */
};

struct rtd1295_pcie {
	struct device *dev;
	const struct rtd1295_pcie_data *data;
	void __iomem *base;
	resource_size_t mmio_window;	/* 4K translated window (reg "mmio") */
	void __iomem *mmio;		/* ... and its mapping */
	raw_spinlock_t lock;		/* translate/mask registers + window */
	u64 mem_bus_base;		/* PCI bus address of the MEM window */
	struct clk *clk;
	struct reset_control_bulk_data resets[ARRAY_SIZE(rtd1295_pcie_reset_names)];
	struct gpio_desc *perst;
	u32 chip_rev;
	int max_link_speed;
};

static const u32 rtd1295_pcie1_phy_a[] = {
	0x31810801, 0xb2001101, 0x00100001,
};

static const u32 rtd1295_pcie1_phy_b[] = {
	0x00000003, 0x00900001, 0x40080401, 0x5ac10801, 0x32b40f01, 0x72001101,
};

static const u32 rtd1295_pcie2_phy[] = {
	0x00000003, 0x27e90301, 0x52f50401, 0xead70501, 0x000c0601,
	0xa6530a01, 0xc4662001, 0x8ea01b01, 0x8ea05b01, 0xa84a0101,
	0xb8032b01, 0x27e94301, 0x52f54401, 0xead74501, 0x000c4601,
	0xa6534a01, 0xd4776001, 0xa84a4101, 0xa8036b01, 0x01225a01,
	0x521c0901, 0x501c0901, 0x521c0901, 0x08282401, 0x08286401,
};

static const struct rtd1295_pcie_data rtd1295_pcie1_data = {
	.phy_a = rtd1295_pcie1_phy_a,
	.phy_a_len = ARRAY_SIZE(rtd1295_pcie1_phy_a),
	.phy_b = rtd1295_pcie1_phy_b,
	.phy_b_len = ARRAY_SIZE(rtd1295_pcie1_phy_b),
	.dir_en = 0x00200001,
	.phy_mux_sel_mask = GENMASK(3, 0),
};

static const struct rtd1295_pcie_data rtd1295_pcie2_data = {
	.phy_a = rtd1295_pcie2_phy,
	.phy_a_len = ARRAY_SIZE(rtd1295_pcie2_phy),
	.phy_b = rtd1295_pcie2_phy,
	.phy_b_len = ARRAY_SIZE(rtd1295_pcie2_phy),
	.dir_en = 0x01000001,
	.phy_mux_sel_mask = GENMASK(19, 16),
	.c68_quirk = true,
};

static inline u32 rtd_readl(struct rtd1295_pcie *pcie, u32 reg)
{
	return readl(pcie->base + reg);
}

static inline void rtd_writel(struct rtd1295_pcie *pcie, u32 reg, u32 val)
{
	writel(val, pcie->base + reg);
}

static bool rtd1295_pcie_valid_device(struct pci_bus *bus, unsigned int devfn)
{
	/* Only the link partner's function 0 is reachable. */
	return pci_is_root_bus(bus) && devfn == PCI_DEVFN(0, 0);
}

static u32 rtd1295_pcie_byte_enable(int where, int size)
{
	return GENMASK((where & 3) + size - 1, where & 3);
}

static u32 rtd1295_pcie_data_mask(u32 byte_en)
{
	u32 mask = 0;
	int i;

	for (i = 0; i < 4; i++)
		if (byte_en & BIT(i))
			mask |= GENMASK(i * 8 + 7, i * 8);

	return mask;
}

static bool rtd1295_pcie_link_up(struct rtd1295_pcie *pcie)
{
	return rtd_readl(pcie, RTD_PCIE_MAC_ST) & RTD_PCIE_MAC_ST_LINK;
}

/*
 * Kick the indirect config engine and wait for it. Called under pci_lock.
 * @addr must be dword aligned; @en selects the bytes. No retry on timeout:
 * a transfer that did not complete is not going to complete when re-issued.
 */
static int rtd1295_pcie_cfg_xfer(struct rtd1295_pcie *pcie, u32 indir,
				 u32 addr, u32 en, u32 wdata, bool write,
				 u32 *status)
{
	u32 st;
	int ret;

	rtd_writel(pcie, RTD_PCIE_INDIR_CTR, indir);
	rtd_writel(pcie, RTD_PCIE_CFG_ST,
		   RTD_PCIE_CFG_ST_ERROR | RTD_PCIE_CFG_ST_DONE);
	rtd_writel(pcie, RTD_PCIE_CFG_ADDR, addr);
	if (write)
		rtd_writel(pcie, RTD_PCIE_CFG_WDATA, wdata);
	rtd_writel(pcie, RTD_PCIE_CFG_EN, en);
	rtd_writel(pcie, RTD_PCIE_CFG_CT, RTD_PCIE_CFG_CT_GO);

	ret = readl_poll_timeout_atomic(pcie->base + RTD_PCIE_CFG_ST,
					st, st & RTD_PCIE_CFG_ST_DONE,
					RTD_PCIE_CFG_POLL_US,
					RTD_PCIE_CFG_TIMEOUT_US);
	st = rtd_readl(pcie, RTD_PCIE_CFG_ST);
	*status = st;
	rtd_writel(pcie, RTD_PCIE_CFG_ST,
		   RTD_PCIE_CFG_ST_ERROR | RTD_PCIE_CFG_ST_DONE);

	if (ret)
		return -ETIMEDOUT;
	if (st & RTD_PCIE_CFG_ST_ERROR)
		return -EIO;
	return 0;
}

static void rtd1295_pcie_cfg_warn(struct rtd1295_pcie *pcie, bool write,
				  int where, int err, u32 st)
{
	dev_warn_ratelimited(pcie->dev, "config %s @%#x failed (%s, status %#x)\n",
			     write ? "write" : "read", where,
			     err == -ETIMEDOUT ? "timeout" : "error", st);
}

static int rtd1295_pcie_read_config(struct pci_bus *bus, unsigned int devfn,
				    int where, int size, u32 *val)
{
	struct rtd1295_pcie *pcie = bus->sysdata;
	u32 addr = RTD_PCIE_CFG_ADDRESS(bus->number, devfn, where & ~3);
	u32 byte_en = rtd1295_pcie_byte_enable(where, size);
	u32 en = FIELD_PREP(RTD_PCIE_CFG_EN_BYTE_CNT, byte_en) |
		 RTD_PCIE_CFG_EN_BYTE_EN;
	int retry = 0;
	u32 dllp = 0, st;
	int err;

	if (!rtd1295_pcie_valid_device(bus, devfn) ||
	    !rtd1295_pcie_link_up(pcie)) {
		PCI_SET_ERROR_RESPONSE(val);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (pcie->chip_rev == RTD_CHIP_REV_A00)
		udelay(200);

	for (;;) {
		err = rtd1295_pcie_cfg_xfer(pcie, RTD_PCIE_INDIR_CTR_READ,
					    addr, en, 0, false, &st);
		if (!err)
			*val = (rtd_readl(pcie, RTD_PCIE_CFG_RDATA) &
				rtd1295_pcie_data_mask(byte_en)) >>
			       ((where & 3) * 8);

		/*
		 * A DLLP error during the access corrupts it. As the vendor
		 * driver does, re-issue it, but only if it did complete.
		 */
		dllp = rtd_readl(pcie, RTD_PCIE_DLLP_ERR);
		if (!(dllp & RTD_PCIE_DLLP_ERR_MASK))
			break;
		rtd_writel(pcie, RTD_PCIE_DLLP_ERR, dllp);
		if (err || retry++ >= RTD_PCIE_CFG_DLLP_RETRIES)
			break;
	}

	if (dllp & RTD_PCIE_DLLP_ERR_MASK)
		dev_err_ratelimited(pcie->dev, "config read @%#x: DLLP error %#x (%d retries)\n",
				    where, dllp, retry);
	if (err) {
		rtd1295_pcie_cfg_warn(pcie, false, where, err, st);
		PCI_SET_ERROR_RESPONSE(val);
		return PCIBIOS_SET_FAILED;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int rtd1295_pcie_write_config(struct pci_bus *bus, unsigned int devfn,
				     int where, int size, u32 val)
{
	struct rtd1295_pcie *pcie = bus->sysdata;
	u32 addr = RTD_PCIE_CFG_ADDRESS(bus->number, devfn, where & ~3);
	u32 byte_en = rtd1295_pcie_byte_enable(where, size);
	u32 en, data, st;
	int err;

	if (!rtd1295_pcie_valid_device(bus, devfn) ||
	    !rtd1295_pcie_link_up(pcie))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (pcie->chip_rev == RTD_CHIP_REV_A00)
		udelay(200);

	/*
	 * Same dword address + byte enables as the read path. The vendor
	 * driver passes the unaligned address here; config TLPs carry a
	 * dword register number, so the engine ignores bits [1:0] and the
	 * lane is selected only by the shifted data and the byte enables.
	 */
	data = (val << ((where & 3) * 8)) & rtd1295_pcie_data_mask(byte_en);
	if (size == 4)
		en = RTD_PCIE_CFG_EN_WRITE;
	else
		en = FIELD_PREP(RTD_PCIE_CFG_EN_BYTE_CNT, byte_en) |
		     RTD_PCIE_CFG_EN_BYTE_EN | RTD_PCIE_CFG_EN_WRITE;

	err = rtd1295_pcie_cfg_xfer(pcie, RTD_PCIE_INDIR_CTR_WRITE, addr, en,
				    data, true, &st);
	if (err) {
		rtd1295_pcie_cfg_warn(pcie, true, where, err, st);
		return PCIBIOS_SET_FAILED;
	}

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops rtd1295_pcie_ops = {
	.read = rtd1295_pcie_read_config,
	.write = rtd1295_pcie_write_config,
};

/*
 * Device MMIO through the 4K window.
 *
 * On RTD129x the CPU cannot use the PCI MEM window (0xc0000000/0xc1000000)
 * to reach the card's registers: reads come back as 0. The vendor Wi-Fi
 * drivers instead go through the controller's 4 KiB "mmio" window and pick
 * the 4K page of the BAR with the translate register (0xd04). Offsets whose
 * low 12 bits are 0xcec (both controllers) or 0xc68 (PCIe 2.0 controller)
 * do not work through the window ("1295 hardware issue") and are reached
 * through a 2K page instead.
 */

static struct rtd1295_pcie *rtd1295_pcie_from_pdev(struct pci_dev *pdev)
{
	struct pci_host_bridge *bridge = pci_find_host_bridge(pdev->bus);

	if (!bridge || bridge->ops != &rtd1295_pcie_ops)
		return NULL;

	return pci_host_bridge_priv(bridge);
}

bool rtd1295_pcie_is_rtd(struct pci_dev *pdev)
{
	return rtd1295_pcie_from_pdev(pdev) != NULL;
}
EXPORT_SYMBOL_GPL(rtd1295_pcie_is_rtd);

static inline bool rtd1295_pcie_rbus_hazard(u32 offset)
{
	u32 low = offset & 0xfff;

	return low >= 0x800 && low < 0xa00;
}

/*
 * One access through the window. A read that fails (link down, completion
 * timeout, or persistent DLLP errors) returns all ones, like a master abort
 * on a real MEM window. Nothing is printed with pcie->lock held.
 */
static u32 rtd1295_pcie_mmio_access(struct pci_dev *pdev, u32 offset,
				    int size, u32 val, bool write)
{
	struct rtd1295_pcie *pcie = rtd1295_pcie_from_pdev(pdev);
	u32 low = offset & 0xfff;
	unsigned long flags, rbus_flags;
	void __iomem *addr;
	u32 bar, mask, err = 0, st = 0;
	bool page2k, timeout = false;
	int retry = 0;

	if (WARN_ON_ONCE(!pcie))
		return ~0U;

	/* Without a link every access would end in a completion timeout. */
	if (!rtd1295_pcie_link_up(pcie)) {
		dev_warn_ratelimited(pcie->dev, "MMIO %s %#x: link down\n",
				     write ? "write" : "read", offset);
		return write ? 0 : ~0U;
	}

	bar = lower_32_bits(pci_bus_address(pdev, 2)) &
	      PCI_BASE_ADDRESS_MEM_MASK;
	page2k = low == 0xcec || (pcie->data->c68_quirk && low == 0xc68);
	mask = page2k ? RTD_PCIE_TRANS_MASK_2K : RTD_PCIE_TRANS_MASK_4K;
	addr = pcie->mmio + (offset & ~mask);

	raw_spin_lock_irqsave(&pcie->lock, flags);

	if (page2k)
		rtd_writel(pcie, RTD_PCIE_TRANS_MASK, mask);
	rtd_writel(pcie, RTD_PCIE_TRANS_ADDR, bar | (offset & mask));

	for (;;) {
		if (rtd1295_pcie_rbus_hazard(offset))
			rtd1295_rbus_lock(&rbus_flags);

		if (write) {
			switch (size) {
			case 1:
				writeb(val, addr);
				break;
			case 2:
				writew(val, addr);
				break;
			default:
				writel(val, addr);
				break;
			}
		} else {
			switch (size) {
			case 1:
				val = readb(addr);
				break;
			case 2:
				val = readw(addr);
				break;
			default:
				val = readl(addr);
				break;
			}
		}

		if (rtd1295_pcie_rbus_hazard(offset))
			rtd1295_rbus_unlock(rbus_flags);

		/* The vendor driver checks for errors after reads only. */
		if (write)
			break;

		err = rtd_readl(pcie, RTD_PCIE_DLLP_ERR);
		st = rtd_readl(pcie, RTD_PCIE_DIR_ST);
		if (st & RTD_PCIE_DIR_ST_TIMEOUT) {
			/*
			 * The read never completed. Re-issuing it would only
			 * cost another completion timeout with IRQs off.
			 */
			rtd_writel(pcie, RTD_PCIE_DLLP_ERR, err);
			rtd_writel(pcie, RTD_PCIE_DIR_ST, st);
			timeout = true;
			val = ~0U;
			break;
		}
		if (!(err & RTD_PCIE_DLLP_ERR_MASK))
			break;

		/* Completed, but a DLLP error corrupted it: re-issue. */
		rtd_writel(pcie, RTD_PCIE_DLLP_ERR, err);
		if (retry++ >= RTD_PCIE_MMIO_DLLP_RETRIES) {
			val = ~0U;
			break;
		}
	}

	/* Leave the window on the BAR's first page, as at init. */
	rtd_writel(pcie, RTD_PCIE_TRANS_ADDR, bar);
	if (page2k)
		rtd_writel(pcie, RTD_PCIE_TRANS_MASK, RTD_PCIE_TRANS_MASK_4K);

	raw_spin_unlock_irqrestore(&pcie->lock, flags);

	if (timeout || retry)
		dev_warn_ratelimited(pcie->dev,
				     "MMIO read %#x: %s, status %#x, DLLP error %#x, %d retries\n",
				     offset, timeout ? "timeout" : "DLLP error",
				     st, err, retry);

	return val;
}

u32 rtd1295_pcie_mmio_read(struct pci_dev *pdev, u32 offset, int size)
{
	return rtd1295_pcie_mmio_access(pdev, offset, size, 0, false);
}
EXPORT_SYMBOL_GPL(rtd1295_pcie_mmio_read);

void rtd1295_pcie_mmio_write(struct pci_dev *pdev, u32 offset, u32 val,
			     int size)
{
	rtd1295_pcie_mmio_access(pdev, offset, size, val, true);
}
EXPORT_SYMBOL_GPL(rtd1295_pcie_mmio_write);

static void rtd1295_pcie_assert_resets(struct rtd1295_pcie *pcie)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pcie->resets); i++)
		reset_control_assert(pcie->resets[i].rstc);
}

static int rtd1295_pcie_deassert_resets(struct rtd1295_pcie *pcie)
{
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(pcie->resets); i++) {
		ret = reset_control_deassert(pcie->resets[i].rstc);
		if (ret)
			return ret;
	}

	return 0;
}

static void rtd1295_pcie_power_off(void *data)
{
	struct rtd1295_pcie *pcie = data;

	rtd1295_pcie_assert_resets(pcie);
	clk_disable_unprepare(pcie->clk);
}

/* Pre-B silicon routes the shared SerDes to PCIe through this block. */
static void rtd1295_pcie_a00_phy_mux(struct rtd1295_pcie *pcie,
				     struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *mux;
	u32 val;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-mux");
	if (!res) {
		dev_warn(pcie->dev, "A00 silicon but no \"phy-mux\" reg\n");
		return;
	}

	/* Shared by both controllers: map without claiming the region. */
	mux = devm_ioremap(pcie->dev, res->start, resource_size(res));
	if (!mux)
		return;

	val = readl(mux + RTD_PHY_MUX_CFG14);
	val &= ~RTD_PHY_MUX_CFG14_MASK;
	val |= 1;
	writel(val, mux + RTD_PHY_MUX_CFG14);

	val = readl(mux + RTD_PHY_MUX_SEL);
	val &= ~pcie->data->phy_mux_sel_mask;
	writel(val, mux + RTD_PHY_MUX_SEL);

	writeb(0x51, mux + RTD_PHY_MUX_CFG8);
}

static int rtd1295_pcie_hw_init(struct rtd1295_pcie *pcie,
				struct platform_device *pdev)
{
	const struct rtd1295_pcie_data *data = pcie->data;
	struct device *dev = pcie->dev;
	const u32 *phy;
	unsigned int i, n;
	u32 val;
	int ret;

	if (pcie->chip_rev == RTD_CHIP_REV_A00)
		rtd1295_pcie_a00_phy_mux(pcie, pdev);


	/* Reset the controller (whatever the boot loader left behind). */
	rtd1295_pcie_assert_resets(pcie);
	ret = rtd1295_pcie_deassert_resets(pcie);
	if (ret)
		goto err_resets;

	ret = clk_prepare_enable(pcie->clk);
	if (ret)
		goto err_resets;

	ret = devm_add_action_or_reset(dev, rtd1295_pcie_power_off, pcie);
	if (ret)
		return ret;

	rtd_writel(pcie, RTD_PCIE_SYS_CTR, RTD_PCIE_SYS_CTR_MDIO);

	if (pcie->max_link_speed == 1) {
		val = rtd_readl(pcie, RTD_PCIE_RC_LNKCTL2);
		val &= ~RTD_PCIE_RC_LNKCTL2_TLS;
		val |= PCI_EXP_LNKCTL2_TLS_2_5GT;
		rtd_writel(pcie, RTD_PCIE_RC_LNKCTL2, val);
	}

	/* PHY parameters, through the MDIO bridge */
	if (pcie->chip_rev == RTD_CHIP_REV_A00 ||
	    pcie->chip_rev == RTD_CHIP_REV_A01) {
		phy = data->phy_a;
		n = data->phy_a_len;
	} else {
		phy = data->phy_b;
		n = data->phy_b_len;
	}
	for (i = 0; i < n; i++) {
		rtd_writel(pcie, RTD_PCIE_MDIO_CTR, phy[i]);
		usleep_range(1000, 1100);
	}

	/* Pulse PERST# */
	if (pcie->perst) {
		gpiod_direction_output(pcie->perst, 1);
		msleep(100);
		gpiod_set_value_cansleep(pcie->perst, 0);
	}

	rtd_writel(pcie, RTD_PCIE_SYS_CTR, RTD_PCIE_SYS_CTR_RUN);
	msleep(50);

	rtd_writel(pcie, RTD_PCIE_LINK_INIT, RTD_PCIE_LINK_INIT_VAL);

	ret = readl_poll_timeout(pcie->base + RTD_PCIE_MAC_ST, val,
				 val & RTD_PCIE_MAC_ST_LINK, 1000,
				 RTD_PCIE_LINK_TIMEOUT_US);
	if (ret) {
		dev_info(dev, "link down\n");
		return -ENODEV;
	}
	dev_info(dev, "link up\n");

	/* Make sure DBI is working: enable I/O, memory and bus mastering. */
	rtd_writel(pcie, RTD_PCIE_RC_COMMAND,
		   PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* 4K "mmio" window, translated into the PCI MEM space */
	rtd_writel(pcie, RTD_PCIE_TRANS_BASE, lower_32_bits(pcie->mmio_window));
	rtd_writel(pcie, RTD_PCIE_TRANS_MASK, RTD_PCIE_TRANS_MASK_4K);
	rtd_writel(pcie, RTD_PCIE_TRANS_ADDR, lower_32_bits(pcie->mem_bus_base));

	/* Prevent a hang when a DLLP error occurs. */
	rtd_writel(pcie, RTD_PCIE_DIR_EN, data->dir_en);

	/* Close the root complex's forwarding windows (base > limit). */
	rtd_writel(pcie, RTD_PCIE_RC_MEM_BASE_LIMIT, 0x0000fff0);
	rtd_writel(pcie, RTD_PCIE_RC_PREF_BASE_LIMIT, 0x0000fff0);

	return 0;

err_resets:
	rtd1295_pcie_assert_resets(pcie);
	return ret;
}

static int rtd1295_pcie_read_chip_rev(struct rtd1295_pcie *pcie)
{
	struct regmap *sb2;
	int ret;

	sb2 = syscon_regmap_lookup_by_phandle_optional(pcie->dev->of_node,
						       "realtek,sb2");
	if (IS_ERR(sb2))
		return PTR_ERR(sb2);
	if (!sb2) {
		/* Assume production (B00 or later) silicon. */
		pcie->chip_rev = U32_MAX;
		return 0;
	}

	ret = regmap_read(sb2, RTD_SB2_CHIP_REV, &pcie->chip_rev);
	if (ret)
		return ret;

	dev_dbg(pcie->dev, "chip revision %#x\n", pcie->chip_rev);
	return 0;
}

static int rtd1295_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pci_host_bridge *bridge;
	struct resource_entry *win;
	struct rtd1295_pcie *pcie;
	struct gpio_desc *power;
	struct resource *res;
	unsigned int i;
	int ret;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(*pcie));
	if (!bridge)
		return -ENOMEM;

	pcie = pci_host_bridge_priv(bridge);
	pcie->dev = dev;
	pcie->data = of_device_get_match_data(dev);
	if (!pcie->data)
		return -EINVAL;

	pcie->base = devm_platform_ioremap_resource_byname(pdev, "ctrl");
	if (IS_ERR(pcie->base))
		return PTR_ERR(pcie->base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mmio");
	if (!res)
		return dev_err_probe(dev, -EINVAL, "missing \"mmio\" reg\n");
	pcie->mmio_window = res->start;
	pcie->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->mmio))
		return PTR_ERR(pcie->mmio);
	raw_spin_lock_init(&pcie->lock);

	win = resource_list_first_type(&bridge->windows, IORESOURCE_MEM);
	if (!win)
		return dev_err_probe(dev, -EINVAL, "no MEM window in ranges\n");
	pcie->mem_bus_base = win->res->start - win->offset;

	pcie->clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(pcie->clk))
		return dev_err_probe(dev, PTR_ERR(pcie->clk), "failed to get clock\n");

	for (i = 0; i < ARRAY_SIZE(pcie->resets); i++)
		pcie->resets[i].id = rtd1295_pcie_reset_names[i];
	ret = devm_reset_control_bulk_get_optional_exclusive(dev,
							     ARRAY_SIZE(pcie->resets),
							     pcie->resets);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get resets\n");

	power = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(power))
		return dev_err_probe(dev, PTR_ERR(power), "failed to get power GPIO\n");

	pcie->perst = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(pcie->perst))
		return dev_err_probe(dev, PTR_ERR(pcie->perst),
				     "failed to get PERST# GPIO\n");

	ret = rtd1295_pcie_read_chip_rev(pcie);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read chip revision\n");

	pcie->max_link_speed = of_pci_get_max_link_speed(dev->of_node);

	ret = rtd1295_pcie_hw_init(pcie, pdev);
	if (ret)
		return ret;

	bridge->sysdata = pcie;
	bridge->ops = &rtd1295_pcie_ops;

	return pci_host_probe(bridge);
}

static const struct of_device_id rtd1295_pcie_of_match[] = {
	{ .compatible = "realtek,rtd1295-pcie1", .data = &rtd1295_pcie1_data },
	{ .compatible = "realtek,rtd1295-pcie2", .data = &rtd1295_pcie2_data },
	{ /* sentinel */ }
};

static struct platform_driver rtd1295_pcie_driver = {
	.driver = {
		.name = "rtd1295-pcie",
		.of_match_table = rtd1295_pcie_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = rtd1295_pcie_probe,
};
builtin_platform_driver(rtd1295_pcie_driver);
