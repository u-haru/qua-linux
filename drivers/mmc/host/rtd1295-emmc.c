// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 eMMC host driver (native, vendor command flow).
 *
 * The controller is a Synopsys DW MSHC with a Realtek wrapper. The generic
 * dw_mmc driver hangs the SoC after a data timeout (its controller resets),
 * so this driver follows the vendor driver's way of using it instead:
 *
 *  - one request at a time, synchronously, from a work item;
 *  - IDMAC chained descriptors exactly as the vendor built them (4 KiB each,
 *    interrupt-on-completion disabled), completion = DW "data over" plus the
 *    wrapper DMA_DONE bit polled for reads;
 *  - an rbus sync (write to SB2 0x9801a020, realtek,sb2) after the
 *    descriptors and buffers are written and before the command starts:
 *    the vendor kernel did this inside every wmb()/writel()
 *    (CONFIG_RTK_RBUS_BARRIER), mainline does not;
 *  - multi-block transfers end with the CMD12 from the core, never CMD23;
 *  - NO controller/FIFO/DMA reset bits (CTRL[2:0]) after errors: the vendor
 *    driver never used them.  On an error the card is stopped with CMD12 and
 *    the host waits for the state machines to go idle.  If they do not (or
 *    data is left in the FIFO), the block is reset the vendor way,
 *    card_stop(): CRT soft reset rstn_emmc (CRT 0x98000004 bit 11) with the
 *    eMMC clocks gated, A01 SYS_DUMMY/DUMMY_SYS ECO bits, then the pad mux /
 *    pad drive registers at 0x98012600 (wiped by that reset) and the
 *    controller registers are restored.  Only if the controller does not
 *    take a clock-update command even after that is the host marked dead.
 *
 * The driver issues commands on its own only for error recovery (CMD12,
 * CMD13) and HS200 tuning (CMD13, CMD18 reads); it never writes the card
 * unless the block layer writes.
 *
 * Card clock: PLL_EMMC in the CRT (realtek,crt syscon) feeds cclk_in, the DW
 * CLKDIV divides it.  Legacy/HS: N = 0x57 (~101 MHz), CLKDIV 1 -> ~50 MHz.
 *
 * HS200 (optional): enabled by DT "mmc-hs200-1_8v" or by the
 * module parameter rtd1295_emmc.hs200=1 (hs200=0 forces it off).  It follows
 * the vendor 4.9 driver (rtkemmc.c, speed-step 2, mmc_Tuning_HS200 +
 * rtkemmc_phase_tuning): PLL N = 0xa6 (~190 MHz), CLKDIV 0, pad drive
 * pddrive_nf_s2 (realtek,drive-strength-hs200), phases (0,0) + 5 ms, then
 *   TX sweep of PLL_EMMC1 VP0 0..31 with CMD13 (only RTO/DCRC/EBE fail),
 *   RX sweep of VP1 0..31 with CMD18 of 2 x 512 bytes at block 0x100 (auto
 *   stop, CDTHRCTL 512, IDMAC + DMA_DONE; a failed read is followed by
 *   CMD12 if the auto stop did not finish, card_stop() and CMD13 polling
 *   to TRAN, exactly as rtkemmc_send_cmd18() does),
 *   TX pass 2 inside the TX window: the vendor wrote CMD25 here; this
 *   driver uses the same CMD18 read (read-only).
 * CMD21 is never used.  The eMMC I/O is always 1.8 V on RTD129x (UHS_REG = 1,
 * PAD_CTL = 0 from probe on, as the vendor did; there is no VCCQ switch).
 * If the HS200 switch or tuning fails, HS200 is dropped from the host caps,
 * the controller is left usable and the core's next rescan attempt brings
 * the card up in HS 52 MHz.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/soc/realtek/rtd1295-rbus.h>
#include <linux/reset.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>

/* DW MSHC registers */
#define EMMC_CTRL		0x000
#define  CTRL_INT_ENABLE	BIT(4)
#define  CTRL_USE_IDMAC		BIT(25)
#define EMMC_PWREN		0x004
#define EMMC_CLKDIV		0x008
#define EMMC_CLKSRC		0x00c
#define EMMC_CLKENA		0x010
#define  CLKENA_ON		0x00010001	/* enable + low power */
#define EMMC_TMOUT		0x014
#define EMMC_CTYPE		0x018
#define  CTYPE_4BIT		BIT(0)
#define  CTYPE_8BIT		BIT(16)
#define EMMC_BLKSIZ		0x01c
#define EMMC_BYTCNT		0x020
#define EMMC_INTMASK		0x024
#define EMMC_CMDARG		0x028
#define EMMC_CMD		0x02c
#define  CMD_START		BIT(31)
#define  CMD_USE_HOLD_REG	BIT(29)
#define  CMD_UPD_CLK		BIT(21)
#define  CMD_INIT		BIT(15)
#define  CMD_STOP_ABORT		BIT(14)
#define  CMD_SEND_AUTO_STOP	BIT(12)
#define  CMD_PRV_DAT_WAIT	BIT(13)
#define  CMD_DAT_WR		BIT(10)
#define  CMD_DAT_EXP		BIT(9)
#define  CMD_RESP_CRC		BIT(8)
#define  CMD_RESP_LONG		BIT(7)
#define  CMD_RESP_EXP		BIT(6)
#define EMMC_RESP0		0x030
#define EMMC_MINTSTS		0x040
#define EMMC_RINTSTS		0x044
#define  INT_EBE		BIT(15)
#define  INT_ACD		BIT(14)	/* auto command done */
#define  INT_SBE		BIT(13)
#define  INT_HLE		BIT(12)
#define  INT_FRUN		BIT(11)
#define  INT_HTO		BIT(10)
#define  INT_DRTO		BIT(9)
#define  INT_RTO		BIT(8)
#define  INT_DCRC		BIT(7)
#define  INT_RCRC		BIT(6)
#define  INT_DTO		BIT(3)
#define  INT_CD			BIT(2)
#define  INT_RE			BIT(1)
#define  INT_CMD_ERR		(INT_RTO | INT_RCRC | INT_RE | INT_HLE)
#define  INT_DATA_ERR		(INT_EBE | INT_SBE | INT_FRUN | INT_HTO | \
				 INT_DRTO | INT_DCRC)
#define EMMC_STATUS		0x048
#define  STATUS_FIFO_EMPTY	BIT(2)
#define  STATUS_DATA_BUSY	BIT(9)
#define  STATUS_DATA_SM_BUSY	BIT(10)
#define EMMC_FIFOTH		0x04c
#define EMMC_UHS_REG		0x074
#define EMMC_BMOD		0x080
#define  BMOD_SWR		BIT(0)
#define  BMOD_DE		BIT(7)
#define EMMC_DBADDR		0x088
#define EMMC_IDSTS		0x08c
#define EMMC_IDINTEN		0x090
#define EMMC_CDTHRCTL		0x100
#define  CDTHRCTL_RD_EN		BIT(0)
#define  CDTHRCTL_THLD		GENMASK(27, 16)
#define  CDTHRCTL_DEFAULT	0x02000001	/* 512 bytes, vendor value */
#define EMMC_DDR_REG		0x10c

/* Realtek wrapper */
#define EMMC_CP			0x41c
#define EMMC_ISR		0x424
#define  ISR_WRITE_DATA		BIT(0)
#define  ISR_DMA_DONE		BIT(1)
#define  ISR_DMA_INT_MASK	BIT(2)
#define  ISR_DESC_INT_MASK	BIT(3)
#define  ISR_IP_INT_MASK	BIT(4)
#define EMMC_DUMMY_SYS		0x42c
#define  DUMMY_SYS_PLL_ECO	BIT(30)
#define EMMC_PAD_CTL		0x474
#define EMMC_CKGEN_CTL		0x478
#define  CKGEN_DEFAULT		0x2100
#define  CKGEN_SRC_MASK		GENMASK(18, 16)
#define  CKGEN_SRC_4MHZ		FIELD_PREP(CKGEN_SRC_MASK, 7)
#define EMMC_DQS_CTRL1		0x498
#define EMMC_SWC_SEL		0x4d4

/* CRT */
#define CRT_SOFT_RESET2		0x004
#define  RSTN_EMMC		BIT(11)		/* active low */
#define CRT_SYS_DUMMY		0x450
#define  SYS_DUMMY_EMMC		BIT(1)		/* vendor card_stop(), A01 */
#define CRT_PLL_EMMC1		0x1f0
#define  PLL_EMMC1_RSTB		BIT(1)
#define  PLL_EMMC1_VP0		GENMASK(7, 3)
#define  PLL_EMMC1_VP1		GENMASK(12, 8)
#define CRT_PLL_EMMC3		0x1f8
#define  PLL_EMMC3_N		GENMASK(31, 16)
#define CRT_PLL_EMMC4		0x1fc
#define  PLL_EMMC4_OEB		BIT(0)
#define RTD_PLL_N_HS		0x57
#define RTD_PLL_N_HS200		0xa6
#define PHASE_KEEP		0xff
#define PHASE_NUM		32
#define RTD_PLL_RATE(n)		((u32)(((n) + 3) * 27000000ULL / 24))

/* SB2 rbus sync, chip revision */
#define SB2_RBUS_SYNC		0x020
#define SB2_CHIP_REV		0x204
#define  CHIP_REV_A01		0x00010000
#define  CHIP_REV_B00		0x00020000

/* CR pin mux at eMMC base + 0x600 (wiped by rstn_emmc, vendor gRegTbl) */
#define CR_MUX_OFFSET		0x600
#define CR_MUX_SIZE		0x14
static const u32 cr_mux_regs[] = { 0x00, 0x04, 0x0c, 0x10 };

/* pad drive (realtek,pad-syscon = 0x98012620) */
#define PAD_PDRIVE_NF1		0x04
#define PAD_PDRIVE_NF2		0x08
#define PAD_PDRIVE_NF3		0x0c
#define PAD_PDRIVE_NF4		0x10
#define PAD_NUM_REGS		9	/* NF0..NF4, CR0, CR1, SDIO, SDIO1 */

/* IDMAC descriptors, vendor format */
#define DES0_OWN		BIT(31)
#define DES0_CH			BIT(4)
#define DES0_FS			BIT(3)
#define DES0_LD			BIT(2)
#define DES0_DIC		BIT(1)
#define DESC_MAX_LEN		4096
#define DESC_NUM		256

#define CMD_TIMEOUT_MS		500
#define DATA_TIMEOUT_MS		3000
/*
 * Longest R1B busy the driver waits for in rtd_emmc_do_cmd() (DAT0 polled).
 * Advertised as max_busy_timeout: the core sends longer operations (erase,
 * cache flush, sanitize, ...) as R1 and polls busy itself (->card_busy).
 */
#define BUSY_TIMEOUT_MS		3000

/* vendor rtkemmc_send_cmd18(emmc_port, 1024, 0x100) */
#define TUNE_BLK_ADDR		0x100
#define TUNE_LEN		1024
#define TUNE_FILL		0xdeadbeef

struct rtd_emmc_desc {
	__le32 des0, des1, des2, des3;
};

struct rtd_emmc_host {
	struct device *dev;
	struct mmc_host *mmc;
	void __iomem *base;
	struct regmap *crt;
	struct regmap *sb2;		/* optional rbus sync */
	struct regmap *pad;		/* optional */
	int irq;

	struct mutex lock;
	struct mmc_request *mrq;
	struct work_struct req_work;
	struct completion done;
	u32 rintsts;
	bool dead;

	struct rtd_emmc_desc *desc;
	dma_addr_t desc_dma;

	u32 bus_hz;
	u32 pll_n;
	u32 phase_hs[2];
	u32 drive[4];
	u32 drive_hs200[4];
	bool drive_valid;
	bool keep_clock;
	unsigned int clock;
	unsigned char timing;
	u32 cdthrctl;
	u32 rca;
	bool hs200_tried;
	bool tuned;
	bool init_sent;
	struct clk *clk_emmc;
	struct clk *clk_ip;
	struct reset_control *rst;	/* optional; else CRT bit directly */
	void __iomem *mux;		/* CR pin mux, optional */
	u32 chip_rev;
	unsigned int resets;		/* card_stop() count */

	u32 *tune_buf;			/* TUNE_LEN, coherent */
	dma_addr_t tune_dma;
	u8 *tune_ref;
	bool tune_ref_valid;
};

static unsigned int max_hz;
module_param(max_hz, uint, 0444);
MODULE_PARM_DESC(max_hz, "cap the eMMC card clock (Hz), 0 = no cap");

static bool keep_boot_clock;
module_param(keep_boot_clock, bool, 0444);
MODULE_PARM_DESC(keep_boot_clock, "never touch PLL_EMMC / phases");

static int hs200 = -1;
module_param(hs200, int, 0444);
MODULE_PARM_DESC(hs200, "HS200: -1 = as DT mmc-hs200-1_8v (default), 1 = on, 0 = off (HS 52 MHz)");

/*
 * RTD129x A01 rbus erratum: every eMMC register access is serialised with
 * the other rbus masters' (PCIe card MMIO) through the shared lock.
 */
#define rtd_bus_lock(f)		rtd1295_rbus_lock(&(f))
#define rtd_bus_unlock(f)	rtd1295_rbus_unlock(f)

static u32 er(struct rtd_emmc_host *h, u32 reg)
{
	unsigned long flags;
	u32 val;

	rtd_bus_lock(flags);
	val = readl(h->base + reg);
	rtd_bus_unlock(flags);
	return val;
}

static void ew(struct rtd_emmc_host *h, u32 reg, u32 val)
{
	unsigned long flags;

	rtd_bus_lock(flags);
	writel(val, h->base + reg);
	rtd_bus_unlock(flags);
}

/* make CPU writes to DRAM visible to rbus masters (vendor rtk_bus_sync) */
static void rtd_emmc_rbus_sync(struct rtd_emmc_host *h)
{
	wmb();	/* CPU stores done before the sync write */
	if (h->sb2)
		regmap_write(h->sb2, SB2_RBUS_SYNC, 0x1234);
	wmb();	/* sync write done before the DMA is started */
}

static int rtd_emmc_wait_not_busy(struct rtd_emmc_host *h, unsigned int ms)
{
	u32 val;

	return read_poll_timeout(er, val,
				  !(val & STATUS_DATA_BUSY), 20, ms * 1000, false, h, EMMC_STATUS);
}

/* issue a command-register-only operation (clock update) and wait for it */
static int rtd_emmc_ciu_cmd(struct rtd_emmc_host *h, u32 cmd)
{
	u32 val;
	int ret;

	ew(h, EMMC_CMD, cmd | CMD_START);
	ret = read_poll_timeout(er, val, !(val & CMD_START),
				 10, 100000, false, h, EMMC_CMD);
	if (ret) {
		dev_err(h->dev, "CIU did not take command %#x, host disabled\n",
			cmd);
		h->dead = true;
	}
	return ret;
}

static void rtd_emmc_clk_park(struct rtd_emmc_host *h, bool park)
{
	u32 val = er(h, EMMC_CKGEN_CTL);

	if (park)
		val |= CKGEN_SRC_4MHZ;
	else
		val &= ~CKGEN_SRC_MASK;
	ew(h, EMMC_CKGEN_CTL, val);
}

/*
 * Vendor rtkemmc_set_freq(): emmc_ip clock gated, PLL_EMMC4 OEB low, new N
 * in PLL_EMMC3[31:16], OEB high, A01 ECO toggle of DUMMY_SYS bit 30, 400 us.
 * The wrapper clock is parked on 4 MHz around it as phase() does.
 */
static void rtd_emmc_set_pll_n(struct rtd_emmc_host *h, u32 n)
{
	if (h->pll_n == n)
		return;

	rtd_emmc_wait_not_busy(h, 100);
	rtd_emmc_clk_park(h, true);
	clk_disable_unprepare(h->clk_ip);

	regmap_update_bits(h->crt, CRT_PLL_EMMC4, PLL_EMMC4_OEB, 0);
	regmap_update_bits(h->crt, CRT_PLL_EMMC3, PLL_EMMC3_N,
			   FIELD_PREP(PLL_EMMC3_N, n));
	regmap_update_bits(h->crt, CRT_PLL_EMMC4, PLL_EMMC4_OEB,
			   PLL_EMMC4_OEB);
	ew(h, EMMC_DUMMY_SYS, er(h, EMMC_DUMMY_SYS) ^ DUMMY_SYS_PLL_ECO);
	udelay(400);

	if (clk_prepare_enable(h->clk_ip))
		dev_err(h->dev, "emmc_ip clock did not come back\n");
	rtd_emmc_clk_park(h, false);

	h->pll_n = n;
	h->bus_hz = RTD_PLL_RATE(n);
}

/* Vendor phase(): VP0 = TX, VP1 = RX, 0..31, PHASE_KEEP leaves one alone. */
static void rtd_emmc_set_phase(struct rtd_emmc_host *h, u32 vp0, u32 vp1)
{
	u32 mask = 0, val = 0;

	if (vp0 != PHASE_KEEP) {
		mask |= PLL_EMMC1_VP0;
		val |= FIELD_PREP(PLL_EMMC1_VP0, vp0);
	}
	if (vp1 != PHASE_KEEP) {
		mask |= PLL_EMMC1_VP1;
		val |= FIELD_PREP(PLL_EMMC1_VP1, vp1);
	}
	if (!mask)
		return;

	rtd_emmc_wait_not_busy(h, 100);
	rtd_emmc_clk_park(h, true);
	regmap_update_bits(h->crt, CRT_PLL_EMMC1, PLL_EMMC1_RSTB, 0);
	regmap_update_bits(h->crt, CRT_PLL_EMMC1, mask, val);
	regmap_update_bits(h->crt, CRT_PLL_EMMC1, PLL_EMMC1_RSTB,
			   PLL_EMMC1_RSTB);
	udelay(200);
	rtd_emmc_clk_park(h, false);
}

static void rtd_emmc_pll_set(struct rtd_emmc_host *h, u32 n, u32 vp0, u32 vp1)
{
	rtd_emmc_set_pll_n(h, n);
	rtd_emmc_set_phase(h, vp0, vp1);
}

static void rtd_emmc_set_clock(struct rtd_emmc_host *h, unsigned int hz)
{
	u32 div;

	if (h->dead)
		return;

	rtd_emmc_wait_not_busy(h, 100);
	ew(h, EMMC_CLKENA, 0);
	if (rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
			     CMD_USE_HOLD_REG))
		return;

	if (!hz) {
		h->mmc->actual_clock = 0;
		h->clock = 0;
		return;
	}

	div = (hz >= h->bus_hz) ? 0 : DIV_ROUND_UP(h->bus_hz, 2 * hz);
	if (div > 0xff)
		div = 0xff;

	ew(h, EMMC_CLKDIV, div);
	if (rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
			     CMD_USE_HOLD_REG))
		return;
	ew(h, EMMC_CLKENA, CLKENA_ON);
	if (rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
			     CMD_USE_HOLD_REG))
		return;

	h->mmc->actual_clock = div ? h->bus_hz / (2 * div) : h->bus_hz;
	h->clock = hz;
	dev_dbg(h->dev, "clock %u Hz (cclk_in %u, div %u)\n",
		h->mmc->actual_clock, h->bus_hz, div);
}

/* vendor rtkemmc_set_pad_driving(): drv = <clk cmd data ds> */
static void rtd_emmc_set_drive(struct rtd_emmc_host *h, const u32 *drv)
{
	u32 data;

	if (!h->pad || !h->drive_valid)
		return;

	data = drv[2] * 0x01010101U;
	regmap_write(h->pad, PAD_PDRIVE_NF1, data);
	regmap_write(h->pad, PAD_PDRIVE_NF2, data);
	regmap_update_bits(h->pad, PAD_PDRIVE_NF3, 0xff00ff00,
			   (drv[0] << 8) | (drv[1] << 24));
	regmap_write(h->pad, PAD_PDRIVE_NF4, drv[3]);
}

static irqreturn_t rtd_emmc_irq(int irq, void *dev_id)
{
	struct rtd_emmc_host *h = dev_id;
	u32 mint = er(h, EMMC_MINTSTS);

	if (!mint)
		return IRQ_NONE;

	/* stop the level interrupt; status stays in RINTSTS for the waiter */
	ew(h, EMMC_INTMASK, 0);
	h->rintsts = er(h, EMMC_RINTSTS);
	complete(&h->done);
	return IRQ_HANDLED;
}

/* enable @mask, wait until any bit of @want or an error shows up */
static u32 rtd_emmc_wait_int(struct rtd_emmc_host *h, u32 mask, u32 want,
			     unsigned int ms)
{
	u32 st = er(h, EMMC_RINTSTS);

	if (st & (want | mask))
		return st;

	reinit_completion(&h->done);
	ew(h, EMMC_INTMASK, mask | want);
	/* close the race with a status that arrived before INTMASK */
	st = er(h, EMMC_RINTSTS);
	if (!(st & (want | mask)) &&
	    !wait_for_completion_timeout(&h->done, msecs_to_jiffies(ms))) {
		ew(h, EMMC_INTMASK, 0);
		return er(h, EMMC_RINTSTS);	/* caller sees "nothing" */
	}
	ew(h, EMMC_INTMASK, 0);
	return er(h, EMMC_RINTSTS);
}

static int rtd_emmc_prep_dma(struct rtd_emmc_host *h, struct mmc_data *data)
{
	struct rtd_emmc_desc *d = h->desc;
	struct scatterlist *sg;
	int i, n, nents;

	nents = dma_map_sg(h->dev, data->sg, data->sg_len,
			   mmc_get_dma_dir(data));
	if (!nents)
		return -ENOMEM;
	data->sg_count = nents;

	n = 0;
	for_each_sg(data->sg, sg, nents, i) {
		dma_addr_t addr = sg_dma_address(sg);
		u32 len = sg_dma_len(sg);

		while (len) {
			u32 chunk = min_t(u32, len, DESC_MAX_LEN);
			u32 des0 = DES0_OWN | DES0_CH | DES0_DIC;

			if (n >= DESC_NUM) {
				dma_unmap_sg(h->dev, data->sg, data->sg_len,
					     mmc_get_dma_dir(data));
				return -EINVAL;
			}
			if (n == 0)
				des0 |= DES0_FS;
			d[n].des0 = cpu_to_le32(des0);
			d[n].des1 = cpu_to_le32(chunk);
			d[n].des2 = cpu_to_le32(lower_32_bits(addr));
			d[n].des3 = cpu_to_le32(lower_32_bits(h->desc_dma) +
						(n + 1) * sizeof(*d));
			addr += chunk;
			len -= chunk;
			n++;
		}
	}
	d[n - 1].des0 |= cpu_to_le32(DES0_LD);

	/* descriptors and buffers must reach DRAM before the IDMAC fetches */
	rtd_emmc_rbus_sync(h);
	return 0;
}

/*
 * DW register set-up shared by probe and card_stop(), in the order of the
 * vendor rtkemmc_chk_card_insert() / mmc_host_reset().  No CTRL reset bits.
 */
static int rtd_emmc_ctrl_init(struct rtd_emmc_host *h)
{
	ew(h, EMMC_DDR_REG, 0);
	ew(h, EMMC_DQS_CTRL1, 0);
	ew(h, EMMC_SWC_SEL, 0);
	ew(h, EMMC_CP, 0);

	/* wrapper: wrapper DMA/descriptor interrupts masked, IP passes */
	ew(h, EMMC_ISR, ISR_WRITE_DATA | ISR_DMA_INT_MASK | ISR_DESC_INT_MASK);
	ew(h, EMMC_ISR, ISR_IP_INT_MASK);
	ew(h, EMMC_ISR, ISR_DMA_DONE);

	ew(h, EMMC_BMOD, BMOD_DE | BMOD_SWR);
	ew(h, EMMC_CTRL, CTRL_USE_IDMAC);
	ew(h, EMMC_PWREN, 1);
	ew(h, EMMC_INTMASK, 0);
	ew(h, EMMC_RINTSTS, 0xffffffff);
	ew(h, EMMC_CLKDIV, 0x80);
	ew(h, EMMC_CLKSRC, 0);
	ew(h, EMMC_CLKENA, 0);
	if (rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_USE_HOLD_REG))
		return -EIO;
	ew(h, EMMC_TMOUT, 0xffffff40);
	ew(h, EMMC_CTYPE, 0);
	ew(h, EMMC_FIFOTH, 0x0001007f);
	ew(h, EMMC_CTRL, CTRL_USE_IDMAC | CTRL_INT_ENABLE);
	ew(h, EMMC_BMOD, BMOD_DE);
	ew(h, EMMC_IDINTEN, 0);
	ew(h, EMMC_IDSTS, 0x3ff);
	ew(h, EMMC_UHS_REG, 1);
	ew(h, EMMC_CDTHRCTL, CDTHRCTL_DEFAULT);
	h->cdthrctl = CDTHRCTL_DEFAULT;
	ew(h, EMMC_PAD_CTL, 0);
	return 0;
}

static bool rtd_emmc_is_a01(struct rtd_emmc_host *h)
{
	return h->chip_rev >= CHIP_REV_A01 && h->chip_rev < CHIP_REV_B00;
}

static void rtd_emmc_rstn(struct rtd_emmc_host *h, bool assert)
{
	if (h->rst) {
		if (assert)
			reset_control_assert(h->rst);
		else
			reset_control_deassert(h->rst);
		return;
	}
	regmap_update_bits(h->crt, CRT_SOFT_RESET2, RSTN_EMMC,
			   assert ? 0 : RSTN_EMMC);
}

/*
 * Vendor card_stop() (rtkemmc.c, RTD129x): what the vendor did after every
 * failed command, including each failed tuning read.  rstn_emmc resets the
 * whole eMMC block, the wrapper and the CR pad registers at 0x98012600, so
 * those are saved and written back (gRegTbl backup/restore), then the DW
 * registers are set up again (mmc_host_reset) and the clock is restored.
 * The CRT PLL_EMMC registers (N, phases) are not touched by this reset.
 * Bounded: every wait inside has a timeout.  Caller holds h->lock.
 */
static int rtd_emmc_card_stop(struct rtd_emmc_host *h)
{
	u32 mux[ARRAY_SIZE(cr_mux_regs)], pad[PAD_NUM_REGS];
	u32 ctype, uhs, ddr, thr, clkdiv, ckgen, dqs;
	unsigned long flags;
	bool a01 = rtd_emmc_is_a01(h);
	int i, ret;

	h->resets++;
	dev_dbg(h->dev, "card_stop #%u (STATUS %#x RINTSTS %#x)\n", h->resets,
		er(h, EMMC_STATUS), er(h, EMMC_RINTSTS));

	/* rtkemmc_backup_registers() */
	if (h->mux) {
		rtd_bus_lock(flags);
		for (i = 0; i < ARRAY_SIZE(cr_mux_regs); i++)
			mux[i] = readl(h->mux + cr_mux_regs[i]);
		rtd_bus_unlock(flags);
	}
	if (h->pad)
		for (i = 0; i < PAD_NUM_REGS; i++)
			regmap_read(h->pad, i * 4, &pad[i]);
	ctype = er(h, EMMC_CTYPE);
	uhs = er(h, EMMC_UHS_REG);
	ddr = er(h, EMMC_DDR_REG);
	thr = er(h, EMMC_CDTHRCTL);
	clkdiv = er(h, EMMC_CLKDIV);
	ckgen = er(h, EMMC_CKGEN_CTL);
	dqs = er(h, EMMC_DQS_CTRL1);

	ew(h, EMMC_INTMASK, 0);
	rtd_emmc_rstn(h, true);
	clk_disable_unprepare(h->clk_emmc);
	clk_disable_unprepare(h->clk_ip);
	if (a01)
		regmap_update_bits(h->crt, CRT_SYS_DUMMY, SYS_DUMMY_EMMC, 0);
	rtd_emmc_rstn(h, false);
	if (clk_prepare_enable(h->clk_emmc) || clk_prepare_enable(h->clk_ip))
		dev_err(h->dev, "eMMC clocks did not come back after reset\n");
	if (a01)
		regmap_update_bits(h->crt, CRT_SYS_DUMMY, SYS_DUMMY_EMMC,
				   SYS_DUMMY_EMMC);
	if (h->chip_rev >= CHIP_REV_A01) {
		ew(h, EMMC_DUMMY_SYS, er(h, EMMC_DUMMY_SYS) ^ DUMMY_SYS_PLL_ECO);
		udelay(200);
	}

	/* rtkemmc_restore_registers() part 1: pads, wrapper clock */
	if (h->mux) {
		rtd_bus_lock(flags);
		for (i = 0; i < ARRAY_SIZE(cr_mux_regs); i++)
			writel(mux[i], h->mux + cr_mux_regs[i]);
		rtd_bus_unlock(flags);
	}
	if (h->pad)
		for (i = 0; i < PAD_NUM_REGS; i++)
			regmap_write(h->pad, i * 4, pad[i]);
	ew(h, EMMC_CKGEN_CTL, ckgen);

	/* mmc_host_reset() */
	ret = rtd_emmc_ctrl_init(h);
	if (ret)
		goto out;

	ew(h, EMMC_CTYPE, ctype);
	ew(h, EMMC_UHS_REG, uhs);
	ew(h, EMMC_DDR_REG, ddr);
	ew(h, EMMC_CDTHRCTL, thr);
	h->cdthrctl = thr;
	ew(h, EMMC_DQS_CTRL1, dqs);

	/* clock back on, as card_stop() does it */
	ew(h, EMMC_CLKENA, 0);
	ret = rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
			       CMD_USE_HOLD_REG);
	if (ret)
		goto out;
	ew(h, EMMC_CLKDIV, clkdiv);
	ret = rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
			       CMD_USE_HOLD_REG);
	if (ret)
		goto out;
	if (h->clock) {
		ew(h, EMMC_CLKENA, CLKENA_ON);
		ret = rtd_emmc_ciu_cmd(h, CMD_UPD_CLK | CMD_PRV_DAT_WAIT |
				       CMD_USE_HOLD_REG);
	}
out:
	if (ret)
		dev_err(h->dev, "controller reset (rstn_emmc) did not help\n");
	return ret;
}

/* CMD12 with stop_abort, bounded; returns RINTSTS after it */
static u32 rtd_emmc_send_abort(struct rtd_emmc_host *h)
{
	u32 val;

	if (read_poll_timeout(er, val, !(val & CMD_START), 10, 100000, false,
			      h, EMMC_CMD))
		return 0;
	ew(h, EMMC_RINTSTS, 0xffff);
	ew(h, EMMC_CMDARG, 0);
	ew(h, EMMC_CMD, MMC_STOP_TRANSMISSION | CMD_RESP_EXP | CMD_RESP_CRC |
	   CMD_STOP_ABORT | CMD_USE_HOLD_REG | CMD_START);
	val = rtd_emmc_wait_int(h, INT_CMD_ERR, INT_CD, CMD_TIMEOUT_MS);
	rtd_emmc_wait_not_busy(h, BUSY_TIMEOUT_MS);
	return val;
}

/*
 * After an error: stop the card, wait for the controller to go idle.  If it
 * does not, or the FIFO still holds data, reset the block (card_stop()).
 */
static void rtd_emmc_recover(struct rtd_emmc_host *h, bool data)
{
	u32 val;
	bool reset = false;

	ew(h, EMMC_INTMASK, 0);

	if (read_poll_timeout(er, val, !(val & CMD_START), 100, 100000, false,
			      h, EMMC_CMD)) {
		reset = true;
		goto reset;
	}

	if (data)
		dev_dbg(h->dev, "recovery CMD12 rintsts %#x\n",
			rtd_emmc_send_abort(h));

	if (read_poll_timeout(er, val,
			      !(val & (STATUS_DATA_SM_BUSY | STATUS_DATA_BUSY)),
			      100, 500000, false, h, EMMC_STATUS)) {
		reset = true;
		goto reset;
	}

	/* drop what is left of the transfer in the IDMAC (vendor BMOD sequence) */
	ew(h, EMMC_BMOD, BMOD_DE | BMOD_SWR);
	ew(h, EMMC_BMOD, BMOD_DE);
	ew(h, EMMC_IDSTS, 0x3ff);
	ew(h, EMMC_RINTSTS, 0xffffffff);
	ew(h, EMMC_ISR, ISR_DMA_DONE);
	if (!(er(h, EMMC_STATUS) & STATUS_FIFO_EMPTY))
		reset = true;

reset:
	if (!reset)
		return;
	dev_warn(h->dev,
		 "controller not idle (CMD %#x STATUS %#x RINTSTS %#x IDSTS %#x ISR %#x), resetting eMMC block\n",
		 er(h, EMMC_CMD), er(h, EMMC_STATUS), er(h, EMMC_RINTSTS),
		 er(h, EMMC_IDSTS), er(h, EMMC_ISR));
	if (rtd_emmc_card_stop(h)) {
		dev_err(h->dev, "eMMC controller dead, host disabled\n");
		h->dead = true;
	}
}

static int rtd_emmc_cmd_error(struct mmc_command *cmd, u32 st)
{
	if (st & INT_RTO)
		return -ETIMEDOUT;
	if ((st & INT_RCRC) && (cmd->flags & MMC_RSP_CRC))
		return -EILSEQ;
	if (st & (INT_RE | INT_HLE))
		return -EIO;
	return 0;
}

static int rtd_emmc_data_error(u32 st)
{
	if (st & INT_DCRC)
		return -EILSEQ;
	if (st & (INT_DRTO | INT_HTO))
		return -ETIMEDOUT;
	if (st & (INT_EBE | INT_SBE | INT_FRUN))
		return -EIO;
	return 0;
}

static int rtd_emmc_do_cmd(struct rtd_emmc_host *h, struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	bool is_stop = cmd->opcode == MMC_STOP_TRANSMISSION;
	u32 cmdr, st, val;
	bool sw_timeout;
	int ret, i;

	cmdr = cmd->opcode | CMD_START | CMD_USE_HOLD_REG;
	if (cmd->flags & MMC_RSP_PRESENT) {
		cmdr |= CMD_RESP_EXP;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= CMD_RESP_LONG;
		if (cmd->flags & MMC_RSP_CRC)
			cmdr |= CMD_RESP_CRC;
	}
	/*
	 * As dw_mmc: "wait for previous data" only for data commands.  CMD13
	 * and the other non-data commands must start while the card holds
	 * DAT0 busy (the core polls busy with CMD13), or they would hang
	 * until CMD_TIMEOUT_MS and end in a recovery.
	 */
	if (is_stop)
		cmdr |= CMD_STOP_ABORT;
	else if (data)
		cmdr |= CMD_PRV_DAT_WAIT;
	if (cmd->opcode == MMC_GO_IDLE_STATE || !h->init_sent) {
		cmdr |= CMD_INIT;
		h->init_sent = true;
	}

	ew(h, EMMC_SWC_SEL, 0);
	ew(h, EMMC_CP, 0);
	ew(h, EMMC_RINTSTS, 0xffffffff);
	ew(h, EMMC_ISR, ISR_DMA_DONE);

	if (data) {
		ret = rtd_emmc_prep_dma(h, data);
		if (ret) {
			dev_err_ratelimited(h->dev, "CMD%u: DMA set-up failed (%d)\n",
					    cmd->opcode, ret);
			cmd->error = ret;
			data->error = ret;
			return ret;
		}
		cmdr |= CMD_DAT_EXP;
		if (data->flags & MMC_DATA_WRITE)
			cmdr |= CMD_DAT_WR;
		/*
		 * HS200: the card read threshold (512) must not exceed the
		 * block size, or the 128-byte CMD21 tuning block never starts
		 * (mainline dw_mci_ctrl_thld() does the same).
		 */
		if (h->timing == MMC_TIMING_MMC_HS200) {
			u32 thr = CDTHRCTL_DEFAULT;

			if (data->blksz < 512)
				thr = FIELD_PREP(CDTHRCTL_THLD, data->blksz) |
				      CDTHRCTL_RD_EN;
			if (thr != h->cdthrctl) {
				ew(h, EMMC_CDTHRCTL, thr);
				h->cdthrctl = thr;
			}
		}
		ew(h, EMMC_BLKSIZ, data->blksz);
		ew(h, EMMC_BYTCNT, data->blksz * data->blocks);
		ew(h, EMMC_DBADDR, lower_32_bits(h->desc_dma));
		ew(h, EMMC_BMOD, BMOD_DE);
	}

	/* A01: the vendor driver saw CMDARG writes get lost; retry a few times */
	for (i = 0; i < 3; i++) {
		ew(h, EMMC_CMDARG, cmd->arg);
		if (er(h, EMMC_CMDARG) == cmd->arg)
			break;
	}

	ew(h, EMMC_CMD, cmdr);

	st = rtd_emmc_wait_int(h, INT_CMD_ERR, INT_CD, CMD_TIMEOUT_MS);
	sw_timeout = !(st & (INT_CD | INT_CMD_ERR));
	if (sw_timeout)
		cmd->error = -ETIMEDOUT;
	else
		cmd->error = rtd_emmc_cmd_error(cmd, st);

	if (!cmd->error && (cmd->flags & MMC_RSP_PRESENT)) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = er(h, EMMC_RESP0);
			cmd->resp[2] = er(h, EMMC_RESP0 + 4);
			cmd->resp[1] = er(h, EMMC_RESP0 + 8);
			cmd->resp[0] = er(h, EMMC_RESP0 + 12);
		} else {
			cmd->resp[0] = er(h, EMMC_RESP0);
		}
	}

	if (data) {
		if (!cmd->error) {
			st = rtd_emmc_wait_int(h, INT_DATA_ERR, INT_DTO,
					       DATA_TIMEOUT_MS);
			if (!(st & (INT_DTO | INT_DATA_ERR)))
				data->error = -ETIMEDOUT;
			else
				data->error = rtd_emmc_data_error(st);
		} else {
			data->error = cmd->error;
		}

		/*
		 * Reads: wait until the wrapper has put the data into DRAM.
		 * Without DMA_DONE the buffer may be stale or partly written:
		 * that is an error, not a short success.
		 */
		if (!data->error && (data->flags & MMC_DATA_READ) &&
		    read_poll_timeout(er, val, val & ISR_DMA_DONE, 10, 100000,
				      false, h, EMMC_ISR)) {
			dev_warn_ratelimited(h->dev,
					     "CMD%u: read DMA_DONE not seen (IDSTS %#x STATUS %#x)\n",
					     cmd->opcode, er(h, EMMC_IDSTS),
					     er(h, EMMC_STATUS));
			data->error = -ETIMEDOUT;
		}

		/*
		 * On an error the IDMAC may still own the buffers: stop it
		 * (CMD12 + BMOD_SWR, or the block reset) before they are
		 * unmapped and handed back.
		 */
		if (data->error) {
			dev_dbg(h->dev, "CMD%u data error %d rintsts %#x\n",
				cmd->opcode, data->error, st);
			rtd_emmc_recover(h, true);
		}
		ew(h, EMMC_ISR, ISR_DMA_DONE);

		dma_unmap_sg(h->dev, data->sg, data->sg_len,
			     mmc_get_dma_dir(data));

		if (data->error)
			return data->error;
		data->bytes_xfered = data->blksz * data->blocks;
		return 0;
	}

	if (!cmd->error && cmd->opcode == MMC_SET_RELATIVE_ADDR)
		h->rca = cmd->arg >> 16;

	/*
	 * R1B: the core keeps cmd->busy_timeout <= max_busy_timeout
	 * (BUSY_TIMEOUT_MS).  The CMD12 after a write has no busy_timeout;
	 * mmc_blk polls the card busy after every write anyway, so a long
	 * programming time there is not an error of the stop command.
	 */
	if (!cmd->error && (cmd->flags & MMC_RSP_BUSY) &&
	    rtd_emmc_wait_not_busy(h, BUSY_TIMEOUT_MS) &&
	    !(is_stop && !cmd->busy_timeout))
		cmd->error = -ETIMEDOUT;

	/* plain response errors (RTO/RCRC) need no recovery, a stuck CIU does */
	if (sw_timeout || (st & INT_HLE))
		rtd_emmc_recover(h, false);

	return cmd->error;
}

static void rtd_emmc_req_work(struct work_struct *work)
{
	struct rtd_emmc_host *h = container_of(work, struct rtd_emmc_host,
					       req_work);
	struct mmc_request *mrq = h->mrq;
	struct mmc_command *cmd = mrq->cmd;

	mutex_lock(&h->lock);

	if (h->dead) {
		cmd->error = -EIO;
		if (cmd->data)
			cmd->data->error = -EIO;
		goto done;
	}

	if (mrq->sbc && rtd_emmc_do_cmd(h, mrq->sbc))
		goto done;

	rtd_emmc_do_cmd(h, cmd);

	if (cmd->data && mrq->stop && !mrq->sbc && !h->dead)
		rtd_emmc_do_cmd(h, mrq->stop);

done:
	h->mrq = NULL;
	mutex_unlock(&h->lock);
	mmc_request_done(h->mmc, mrq);
}

static void rtd_emmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct rtd_emmc_host *h = mmc_priv(mmc);

	WARN_ON(h->mrq);
	h->mrq = mrq;
	schedule_work(&h->req_work);
}

/*
 * Vendor rtkemmc_set_ios() + mmc_Tuning_HS200(): HS200 = PLL N 0xa6, CLKDIV 0
 * (set_clock picks it for 200 MHz), pad drive s2.  The phases are left
 * as they are until execute_tuning.  Leaving HS200 restores the HS set-up.
 */
static void rtd_emmc_set_timing(struct rtd_emmc_host *h, unsigned char timing)
{
	if (timing == MMC_TIMING_MMC_HS200) {
		h->hs200_tried = true;
		rtd_emmc_set_drive(h, h->drive_hs200);
		rtd_emmc_set_pll_n(h, RTD_PLL_N_HS200);
		h->cdthrctl = CDTHRCTL_DEFAULT;
		ew(h, EMMC_CDTHRCTL, CDTHRCTL_DEFAULT);
		dev_dbg(h->dev, "HS200: cclk_in ~%u Hz\n", h->bus_hz);
	} else if (h->timing == MMC_TIMING_MMC_HS200) {
		ew(h, EMMC_CDTHRCTL, CDTHRCTL_DEFAULT);
		rtd_emmc_set_drive(h, h->drive);
		rtd_emmc_pll_set(h, RTD_PLL_N_HS, h->phase_hs[0],
				 h->phase_hs[1]);
	}
	h->timing = timing;
}

/* vendor search_best(): centre of the longest (circular) run of good phases */
static int rtd_emmc_best_phase(u32 window)
{
	int start, len, best = -1, best_len = 0, i;

	if (!window)
		return -1;
	if (window == GENMASK(PHASE_NUM - 1, 0))
		return PHASE_NUM / 2;

	for (start = 0; start < PHASE_NUM; start++) {
		/* only start at the first phase of a run */
		if (!(window & BIT(start)) ||
		    (window & BIT((start + PHASE_NUM - 1) % PHASE_NUM)))
			continue;
		for (len = 0, i = start; len < PHASE_NUM &&
		     (window & BIT(i % PHASE_NUM)); len++, i++)
			;
		if (len > best_len) {
			best_len = len;
			best = (start + (len - 1) / 2) % PHASE_NUM;
		}
	}
	return best;
}

/*
 * TX check as the vendor did it: CMD13, only RINTSTS & 0x8180 (RTO, DCRC,
 * EBE) is a failure; RCRC/RE still mean the card heard the command (the RX
 * phase is not tuned yet).  Caller holds h->lock.
 */
static bool rtd_emmc_tune_tx_ok(struct rtd_emmc_host *h)
{
	struct mmc_command cmd = {
		.opcode = MMC_SEND_STATUS,
		.arg = h->rca << 16,
		.flags = MMC_RSP_R1 | MMC_CMD_AC,
	};
	int err;

	if (h->dead)
		return false;
	err = rtd_emmc_do_cmd(h, &cmd);
	return !h->dead && err != -ETIMEDOUT;
}

/* vendor polling_to_tran_state(): CMD13 x5, else CMD12 + wait for TRAN */
static void rtd_emmc_to_tran(struct rtd_emmc_host *h)
{
	struct mmc_command cmd = {
		.opcode = MMC_SEND_STATUS,
		.arg = h->rca << 16,
		.flags = MMC_RSP_R1 | MMC_CMD_AC,
	};
	struct mmc_command stop = {
		.opcode = MMC_STOP_TRANSMISSION,
		.flags = MMC_RSP_R1B | MMC_CMD_AC,
	};
	int err = -EIO, i;

	for (i = 0; i < 5 && err && !h->dead; i++)
		err = rtd_emmc_do_cmd(h, &cmd);
	if (h->dead || (!err && R1_CURRENT_STATE(cmd.resp[0]) == R1_STATE_TRAN))
		return;

	rtd_emmc_do_cmd(h, &stop);
	for (i = 0; i < 20 && !h->dead; i++) {
		if (!rtd_emmc_do_cmd(h, &cmd) &&
		    R1_CURRENT_STATE(cmd.resp[0]) == R1_STATE_TRAN)
			return;
		usleep_range(1000, 2000);
	}
	dev_dbg(h->dev, "card not back in TRAN after a tuning error\n");
}

/*
 * Vendor rtkemmc_send_cmd18(emmc_port, 1024, 0x100): CMD18 with auto stop,
 * 2 blocks of 512 bytes from block 0x100 (sector address: the 8 GB eMMC is
 * high capacity) into a buffer pre-filled with 0xdeadbeef, one IDMAC
 * descriptor, CDTHRCTL 512 (0x02000001), done = ACD + DMA_DONE + not busy.
 * On error: 200 us, CMD12 unless the auto stop already completed,
 * card_stop() (rstn_emmc) and CMD13 polling back to TRAN.
 * Caller holds h->lock.
 */
static int rtd_emmc_tune_read(struct rtd_emmc_host *h)
{
	struct rtd_emmc_desc *d = h->desc;
	u32 cmdr, st, val;
	int i;

	for (i = 0; i < TUNE_LEN / 4; i++)
		h->tune_buf[i] = TUNE_FILL;
	d[0].des0 = cpu_to_le32(DES0_OWN | DES0_CH | DES0_DIC | DES0_FS |
				DES0_LD);
	d[0].des1 = cpu_to_le32(TUNE_LEN);
	d[0].des2 = cpu_to_le32(lower_32_bits(h->tune_dma));
	d[0].des3 = cpu_to_le32(lower_32_bits(h->desc_dma) + sizeof(*d));
	rtd_emmc_rbus_sync(h);

	ew(h, EMMC_SWC_SEL, 0);
	ew(h, EMMC_CP, 0);
	ew(h, EMMC_RINTSTS, 0xffffffff);
	ew(h, EMMC_ISR, ISR_DMA_DONE);
	if (h->cdthrctl != CDTHRCTL_DEFAULT) {
		ew(h, EMMC_CDTHRCTL, CDTHRCTL_DEFAULT);
		h->cdthrctl = CDTHRCTL_DEFAULT;
	}
	ew(h, EMMC_BLKSIZ, 512);
	ew(h, EMMC_BYTCNT, TUNE_LEN);
	ew(h, EMMC_DBADDR, lower_32_bits(h->desc_dma));
	ew(h, EMMC_BMOD, BMOD_DE);
	for (i = 0; i < 3; i++) {
		ew(h, EMMC_CMDARG, TUNE_BLK_ADDR);
		if (er(h, EMMC_CMDARG) == TUNE_BLK_ADDR)
			break;
	}

	cmdr = MMC_READ_MULTIPLE_BLOCK | CMD_START | CMD_USE_HOLD_REG |
	       CMD_SEND_AUTO_STOP | CMD_DAT_EXP | CMD_RESP_CRC | CMD_RESP_EXP |
	       CMD_PRV_DAT_WAIT;
	ew(h, EMMC_CMD, cmdr);

	st = rtd_emmc_wait_int(h, INT_CMD_ERR, INT_CD, CMD_TIMEOUT_MS);
	if (!(st & INT_CD) || (st & INT_CMD_ERR))
		goto fail;
	/* data, then the auto CMD12 (ACD) */
	st = rtd_emmc_wait_int(h, INT_DATA_ERR | INT_CMD_ERR, INT_ACD, 500);
	if (!(st & INT_ACD) || !(st & INT_DTO) ||
	    (st & (INT_DATA_ERR | INT_CMD_ERR)))
		goto fail;
	if (read_poll_timeout(er, val, val & ISR_DMA_DONE, 10, 100000, false,
			      h, EMMC_ISR))
		goto fail;
	if (rtd_emmc_wait_not_busy(h, 500))
		goto fail;
	ew(h, EMMC_ISR, ISR_DMA_DONE);
	return 0;

fail:
	dev_dbg(h->dev, "tuning CMD18 failed: RINTSTS %#x STATUS %#x\n",
		er(h, EMMC_RINTSTS), er(h, EMMC_STATUS));
	ew(h, EMMC_INTMASK, 0);
	udelay(200);
	if (!(er(h, EMMC_RINTSTS) & INT_ACD))
		rtd_emmc_send_abort(h);
	if (rtd_emmc_card_stop(h)) {
		dev_err(h->dev, "eMMC controller dead, host disabled\n");
		h->dead = true;
		return -EIO;
	}
	rtd_emmc_to_tran(h);
	ew(h, EMMC_ISR, ISR_DMA_DONE);
	return -EIO;
}

/*
 * Pass = no error (the vendor's criterion; data is CRC16 protected per
 * line).  On top of that the data must differ from the fill pattern and
 * match the first good read, so a phase that "passes" without the DMA
 * having written the buffer cannot be chosen.
 */
static bool rtd_emmc_tune_rx_ok(struct rtd_emmc_host *h)
{
	int i;

	if (h->dead || rtd_emmc_tune_read(h))
		return false;
	for (i = 0; i < TUNE_LEN / 4; i++)
		if (h->tune_buf[i] != TUNE_FILL)
			break;
	if (i == TUNE_LEN / 4)
		return false;
	if (!h->tune_ref_valid) {
		memcpy(h->tune_ref, h->tune_buf, TUNE_LEN);
		h->tune_ref_valid = true;
		return true;
	}
	return !memcmp(h->tune_ref, h->tune_buf, TUNE_LEN);
}

/*
 * Vendor mmc_Tuning_HS200() + rtkemmc_phase_tuning(mode HS200, range 32).
 * PLL N 0xa6, CLKDIV 0 and the s2 pad drive are already set (set_ios).
 * The core's opcode (CMD21) is not used: the vendor never sent CMD21.
 */
static int rtd_emmc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct rtd_emmc_host *h = mmc_priv(mmc);
	u32 tx = 0, rx = 0, tx1 = 0, val;
	int tx_best = -1, rx_best = -1, tx1_best = -1, i;
	unsigned int resets0;

	if (h->timing != MMC_TIMING_MMC_HS200)
		return -EINVAL;

	mutex_lock(&h->lock);
	if (h->dead)
		goto fail;
	resets0 = h->resets;
	h->tune_ref_valid = false;

	rtd_emmc_set_phase(h, 0, 0);
	usleep_range(5000, 6000);

	for (i = 0; i < PHASE_NUM && !h->dead; i++) {
		rtd_emmc_set_phase(h, i, PHASE_KEEP);
		if (rtd_emmc_tune_tx_ok(h))
			tx |= BIT(i);
	}
	tx_best = rtd_emmc_best_phase(tx);
	if (tx_best < 0 || h->dead)
		goto fail;
	rtd_emmc_set_phase(h, tx_best, PHASE_KEEP);

	for (i = 0; i < PHASE_NUM && !h->dead; i++) {
		rtd_emmc_set_phase(h, PHASE_KEEP, i);
		if (rtd_emmc_tune_rx_ok(h))
			rx |= BIT(i);
	}
	rx_best = rtd_emmc_best_phase(rx);
	if (rx_best < 0 || h->dead)
		goto fail;
	rtd_emmc_set_phase(h, PHASE_KEEP, rx_best);

	/* TX pass 2 (vendor: CMD25 writes; here the same CMD18 read) */
	tx1 = tx;
	for (i = 0; i < PHASE_NUM && !h->dead; i++) {
		if (!(tx & BIT(i)))
			continue;
		rtd_emmc_set_phase(h, i, PHASE_KEEP);
		if (!rtd_emmc_tune_rx_ok(h))
			tx1 &= ~BIT(i);
	}
	tx1_best = rtd_emmc_best_phase(tx1);
	if (tx1_best < 0 || h->dead)
		goto fail;
	rtd_emmc_set_phase(h, tx1_best, PHASE_KEEP);

	/* the chosen pair must work twice in a row */
	for (i = 0; i < 2; i++)
		if (!rtd_emmc_tune_rx_ok(h))
			goto fail;

	regmap_read(h->crt, CRT_PLL_EMMC1, &val);
	dev_info(h->dev,
		 "HS200 tuning: TX %08x RX %08x TX2 %08x -> TX %d RX %d (PLL_EMMC1 %08x, cclk %u Hz, %u resets)\n",
		 tx, rx, tx1, tx1_best, rx_best, val, mmc->actual_clock,
		 h->resets - resets0);
	h->tuned = true;
	mutex_unlock(&h->lock);
	return 0;

fail:
	/*
	 * Leave the controller usable: no transfer pending, FIFO empty.
	 * Then drop HS200; the core powers the card off (set_ios restores
	 * PLL N 0x57, the HS phases and drive) and its next rescan attempt
	 * initialises the card in HS.
	 */
	if (!h->dead &&
	    ((er(h, EMMC_STATUS) & (STATUS_DATA_SM_BUSY | STATUS_DATA_BUSY)) ||
	     !(er(h, EMMC_STATUS) & STATUS_FIFO_EMPTY) ||
	     (er(h, EMMC_CMD) & CMD_START)) &&
	    rtd_emmc_card_stop(h))
		h->dead = true;
	mmc->caps2 &= ~MMC_CAP2_HS200;
	dev_err(h->dev,
		"HS200 tuning failed: TX %08x RX %08x TX2 %08x (%u resets)%s, falling back to HS\n",
		tx, rx, tx1, h->resets, h->dead ? ", controller DEAD" : "");
	mutex_unlock(&h->lock);
	return -EIO;
}

static void rtd_emmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct rtd_emmc_host *h = mmc_priv(mmc);
	bool force_clk = false;
	u32 ctype = 0;

	mutex_lock(&h->lock);
	if (h->dead)
		goto out;

	if (ios->power_mode == MMC_POWER_UP) {
		h->init_sent = false;
		/* last init went to HS200 and never tuned: retry in HS */
		if (h->hs200_tried && !h->tuned &&
		    (mmc->caps2 & MMC_CAP2_HS200)) {
			dev_warn(h->dev, "HS200 failed, falling back to HS\n");
			mmc->caps2 &= ~MMC_CAP2_HS200;
		}
		h->hs200_tried = false;
		h->tuned = false;
	}

	if (ios->timing != h->timing) {
		rtd_emmc_set_timing(h, ios->timing);
		force_clk = true;
	}

	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_8:
		ctype = CTYPE_8BIT;
		break;
	case MMC_BUS_WIDTH_4:
		ctype = CTYPE_4BIT;
		break;
	}
	ew(h, EMMC_CTYPE, ctype);

	if (ios->clock != h->clock || force_clk)
		rtd_emmc_set_clock(h, ios->clock);
out:
	mutex_unlock(&h->lock);
}

/* The eMMC I/O pads are fixed at 1.8 V. */
static int rtd_emmc_switch_voltage(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct rtd_emmc_host *h = mmc_priv(mmc);

	if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_180)
		return -EINVAL;
	if (!h->dead) {
		ew(h, EMMC_PAD_CTL, 0);
		ew(h, EMMC_UHS_REG, 1);
	}
	return 0;
}

static int rtd_emmc_card_busy(struct mmc_host *mmc)
{
	struct rtd_emmc_host *h = mmc_priv(mmc);

	return h->dead ? 0 : !!(er(h, EMMC_STATUS) & STATUS_DATA_BUSY);
}

static const struct mmc_host_ops rtd_emmc_ops = {
	.request			= rtd_emmc_request,
	.set_ios			= rtd_emmc_set_ios,
	.start_signal_voltage_switch	= rtd_emmc_switch_voltage,
	.card_busy			= rtd_emmc_card_busy,
	.execute_tuning			= rtd_emmc_execute_tuning,
};

/* Controller set-up in the order of the vendor rtkemmc_chk_card_insert(). */
static int rtd_emmc_hw_init(struct rtd_emmc_host *h)
{
	u32 val;

	if (rtd_emmc_ctrl_init(h))
		return -EIO;

	if (!h->keep_clock) {
		rtd_emmc_set_drive(h, h->drive);
		rtd_emmc_pll_set(h, RTD_PLL_N_HS, h->phase_hs[0],
				 h->phase_hs[1]);
		ew(h, EMMC_CKGEN_CTL, CKGEN_DEFAULT);
	}

	regmap_read(h->crt, CRT_PLL_EMMC1, &val);
	dev_info(h->dev, "cclk_in ~%u Hz (N %#x), phase TX %lu RX %lu, HS200 %s\n",
		 h->bus_hz, h->pll_n, FIELD_GET(PLL_EMMC1_VP0, val),
		 FIELD_GET(PLL_EMMC1_VP1, val),
		 (h->mmc->caps2 & MMC_CAP2_HS200) ? "on" : "off");
	return 0;
}

static int rtd_emmc_probe(struct platform_device *pdev)
{
	static const char * const clk_names[] = { "emmc", "emmc_ip", "cr" };
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rtd_emmc_host *h;
	struct mmc_host *mmc;
	struct resource *res;
	struct clk *clk;
	u32 val;
	int ret, i;

	mmc = devm_mmc_alloc_host(dev, sizeof(*h));
	if (!mmc)
		return -ENOMEM;
	h = mmc_priv(mmc);
	h->dev = dev;
	h->mmc = mmc;
	mutex_init(&h->lock);
	init_completion(&h->done);
	INIT_WORK(&h->req_work, rtd_emmc_req_work);

	h->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(h->base))
		return PTR_ERR(h->base);
	/*
	 * CR pin mux (owned by the pinctrl driver, not requested here): only
	 * saved/restored around the rstn_emmc reset, like the vendor did.
	 */
	h->mux = devm_ioremap(dev, res->start + CR_MUX_OFFSET, CR_MUX_SIZE);
	if (!h->mux)
		dev_warn(dev, "CR pin mux not mapped, not restored after reset\n");
	h->irq = platform_get_irq(pdev, 0);
	if (h->irq < 0)
		return h->irq;

	h->crt = syscon_regmap_lookup_by_phandle(np, "realtek,crt");
	if (IS_ERR(h->crt))
		return dev_err_probe(dev, PTR_ERR(h->crt), "realtek,crt\n");
	h->sb2 = syscon_regmap_lookup_by_phandle_optional(np, "realtek,sb2");
	if (IS_ERR(h->sb2))
		return PTR_ERR(h->sb2);
	if (!h->sb2)
		dev_warn(dev, "no realtek,sb2: DMA without rbus sync\n");
	h->pad = syscon_regmap_lookup_by_phandle_optional(np,
							  "realtek,pad-syscon");
	if (IS_ERR(h->pad))
		return PTR_ERR(h->pad);

	for (i = 0; i < ARRAY_SIZE(clk_names); i++) {
		clk = devm_clk_get_optional_enabled(dev, clk_names[i]);
		if (IS_ERR(clk))
			return dev_err_probe(dev, PTR_ERR(clk), "%s clock\n",
					     clk_names[i]);
		if (i == 0)
			h->clk_emmc = clk;
		if (i == 1)
			h->clk_ip = clk;
	}

	/* rstn_emmc: from DT if described, else CRT 0x98000004 bit 11 */
	h->rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(h->rst))
		return dev_err_probe(dev, PTR_ERR(h->rst), "reset\n");

	h->chip_rev = CHIP_REV_A01;	/* the Qua Station SoC */
	if (h->sb2 && !regmap_read(h->sb2, SB2_CHIP_REV, &val))
		h->chip_rev = val;

	h->drive_valid = !of_property_read_u32_array(np,
				"realtek,drive-strength-hs", h->drive, 4);
	/* vendor default for RTD129x HS200 when pddrive_nf_s2 is absent */
	if (of_property_read_u32_array(np, "realtek,drive-strength-hs200",
				       h->drive_hs200, 4))
		for (i = 0; i < 4; i++)
			h->drive_hs200[i] = 0xbb;
	h->rca = 1;	/* what the core assigns to (e)MMC */
	of_property_read_u32_array(np, "realtek,phase-hs", h->phase_hs, 2);
	h->phase_hs[0] &= 0x1f;
	h->phase_hs[1] &= 0x1f;
	h->keep_clock = keep_boot_clock ||
			of_property_read_bool(np, "realtek,keep-boot-clock");

	regmap_read(h->crt, CRT_PLL_EMMC3, &val);
	h->pll_n = FIELD_GET(PLL_EMMC3_N, val);
	h->bus_hz = RTD_PLL_RATE(h->pll_n);

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	h->desc = dmam_alloc_coherent(dev, DESC_NUM * sizeof(*h->desc),
				      &h->desc_dma, GFP_KERNEL);
	if (!h->desc)
		return -ENOMEM;
	h->tune_buf = dmam_alloc_coherent(dev, TUNE_LEN, &h->tune_dma,
					  GFP_KERNEL);
	h->tune_ref = devm_kmalloc(dev, TUNE_LEN, GFP_KERNEL);
	if (!h->tune_buf || !h->tune_ref)
		return -ENOMEM;

	ret = mmc_of_parse(mmc);
	if (ret)
		return ret;

	mmc->ops = &rtd_emmc_ops;
	mmc->f_min = 400000;
	if (hs200 > 0)
		mmc->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
	if (!hs200 || h->keep_clock)
		mmc->caps2 &= ~MMC_CAP2_HS200_1_8V_SDR;
	mmc->caps2 &= ~(MMC_CAP2_HS200_1_2V_SDR | MMC_CAP2_HS400);
	if (mmc->caps2 & MMC_CAP2_HS200) {
		if (!mmc->f_max || mmc->f_max > 200000000)
			mmc->f_max = 200000000;
		/*
		 * Allow a second rescan attempt (300 kHz) so that a failed
		 * HS200 init falls back to HS (see set_ios MMC_POWER_UP).
		 */
		mmc->f_min = 200000;
	} else if (!mmc->f_max || mmc->f_max > 52000000) {
		mmc->f_max = 52000000;
	}
	if (max_hz && max_hz < mmc->f_max)
		mmc->f_max = max_hz;
	if (!mmc->ocr_avail)
		mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps &= ~(MMC_CAP_CMD23 | MMC_CAP_UHS | MMC_CAP_1_8V_DDR |
		       MMC_CAP_3_3V_DDR);
	mmc->caps2 |= MMC_CAP2_NO_SD | MMC_CAP2_NO_SDIO;

	mmc->max_busy_timeout = BUSY_TIMEOUT_MS;
	mmc->max_segs = DESC_NUM / 2;
	mmc->max_seg_size = DESC_MAX_LEN;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 1024;
	mmc->max_req_size = 512 * 1024;
	if (mmc->max_req_size > mmc->max_segs * mmc->max_seg_size)
		mmc->max_req_size = mmc->max_segs * mmc->max_seg_size;

	ew(h, EMMC_INTMASK, 0);
	ret = devm_request_irq(dev, h->irq, rtd_emmc_irq, 0, dev_name(dev), h);
	if (ret)
		return ret;

	ret = rtd_emmc_hw_init(h);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, h);
	return mmc_add_host(mmc);
}

static void rtd_emmc_remove(struct platform_device *pdev)
{
	struct rtd_emmc_host *h = platform_get_drvdata(pdev);

	mmc_remove_host(h->mmc);
	cancel_work_sync(&h->req_work);
	ew(h, EMMC_INTMASK, 0);
}

static const struct of_device_id rtd_emmc_of_match[] = {
	{ .compatible = "realtek,rtd1295-emmc" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd_emmc_of_match);

static struct platform_driver rtd_emmc_driver = {
	.probe	= rtd_emmc_probe,
	.remove	= rtd_emmc_remove,
	.driver	= {
		.name		= "rtd1295-emmc",
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table	= rtd_emmc_of_match,
	},
};
module_platform_driver(rtd_emmc_driver);

MODULE_DESCRIPTION("Realtek RTD1295 eMMC host driver");
MODULE_LICENSE("GPL");
