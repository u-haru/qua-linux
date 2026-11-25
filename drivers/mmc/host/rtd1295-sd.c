// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 SD card slot ("card reader", 0x98010400).
 *
 * The SD engine is the byte-register SD core known from the Realtek RTS5xxx
 * card readers (SD_CFG1..3, SD_CMD0..5, SD_TRANSFER with transfer-mode codes)
 * behind a Realtek wrapper that provides a one-shot DMA (start address in
 * 8-byte units, block count) and interrupt gating.  It is not SDHCI/DW-MSHC.
 *
 * One mmc_request is handled at a time, synchronously, from a work item.
 * Data goes through a coherent bounce buffer, so the 8-byte DMA alignment and
 * the one-segment DMA never constrain the block layer.
 *
 * The SD PLL lives in the CRT block (realtek,crt syscon).  Pin mux and pulls
 * come from the CR pin controller: pinctrl state "default" is the card-off
 * state (pull-downs, as the vendor driver used while the card is unpowered),
 * optional state "power-on" (pull-ups on CMD/DAT) is selected while the card
 * is powered.  Pad drive strength (0x98012620, shared with the eMMC) is only
 * touched when the optional realtek,pad-syscon is given.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/scatterlist.h>
#include <linux/unaligned.h>
#include <linux/workqueue.h>

/* Wrapper (32-bit) registers */
#define SD_WRAP_CTL		0x020	/* bit0: DMA reset_n, bit1: L4 gate disable */
#define SD_DMA_CTL1		0x004	/* DMA address >> 3 */
#define SD_DMA_CTL2		0x008	/* block count */
#define SD_DMA_CTL3		0x00c
#define  DMA_XFER		BIT(0)
#define  DMA_DDR_WR		BIT(1)	/* direction: card -> DDR */
#define  DMA_RSP17_SEL		BIT(4)
#define  DMA_DAT64_SEL		BIT(5)
#define SD_ISR			0x024
#define SD_ISREN		0x028
#define  ISR_WRITE_DATA		BIT(0)	/* 1: set the given bits, 0: clear */
#define  ISR_END		BIT(1)
#define  ISR_ERR		BIT(2)
#define  ISR_DMA_DONE		BIT(4)
#define  ISR_ALL		(ISR_END | ISR_ERR | ISR_DMA_DONE)
#define SD_WRAP_DUMMY		0x02c
#define  DUMMY_PLL_CHANGE	BIT(30)	/* set while PLL_SD changes (A01 ECO) */
#define SD_PAD_CTL		0x074	/* 0: 3.3 V */
#define SD_CKGEN_CTL		0x078
#define  CKGEN_BASE		0x2100
#define  CKGEN_DIV_MASK		GENMASK(1, 0)	/* /1 /2 /4 /8 */
#define  CKGEN_SRC_4MHZ		GENMASK(18, 16)

/* SD engine (8-bit) registers */
#define CARD_STOP		0x103
#define CARD_OE			0x104
#define CARD_SELECT		0x10e
#define CARD_EXIST		0x11f
#define  CARD_EXIST_SD		BIT(2)
#define  CARD_EXIST_WP		BIT(5)
#define CARD_CLOCK_EN_CTL	0x129
#define  CARD_CLK_EN_SD		BIT(2)
#define SD_CFG1			0x180
#define  CFG1_DIV_MASK		GENMASK(7, 6)
#define  CFG1_DIV_1		0x00
#define  CFG1_DIV_128		0x80
#define  CFG1_DIV_256		0xc0
#define  CFG1_FIFO_NOT_RST	BIT(4)
#define  CFG1_MODE_MASK		GENMASK(3, 2)	/* 0: SD 2.0 */
#define  CFG1_BUS_MASK		GENMASK(1, 0)
#define  CFG1_BUS_4		0x01
#define SD_CFG2			0x181
#define  CFG2_NO_CRC16		BIT(6)
#define  CFG2_WAIT_BUSY		BIT(3)
#define  CFG2_NO_CRC7_CHK	BIT(2)
#define  CFG2_RSP_NONE		0x00
#define  CFG2_RSP_6B		0x01
#define  CFG2_RSP_17B		0x02
#define SD_CFG3			0x182
#define  CFG3_RSP_CHK		BIT(2)
#define  CFG3_RSP_TIMEOUT	BIT(0)
#define SD_STATUS1		0x183
#define SD_STATUS2		0x184
#define  STATUS2_RSP_TIMEOUT	BIT(0)
#define SD_BUS_STATUS		0x185
#define  BUS_STATUS_DAT0	BIT(1)
#define SD_SAMPLE_POINT_CTL	0x187
#define SD_PUSH_POINT_CTL	0x188
#define SD_CMD0			0x189	/* ..SD_CMD5 = 0x18e */
#define SD_CMD5			0x18e
#define SD_BYTE_CNT_L		0x18f
#define SD_BYTE_CNT_H		0x190
#define SD_BLOCK_CNT_L		0x191
#define SD_BLOCK_CNT_H		0x192
#define SD_TRANSFER		0x193
#define  XFER_START		BIT(7)
#define  XFER_END		BIT(6)
#define  XFER_IDLE		BIT(5)
#define  XFER_ERR		BIT(4)
#define  TM_AUTOWRITE2		0x0a	/* command + write data */
#define  TM_CMD_RSP		0x08	/* command + response */
#define  TM_NORMALREAD		0x0c	/* command + short read (via DMA) */
#define  TM_AUTOREAD2		0x0e	/* command + read data */
#define SD_BUS_TA_STATE		0x197

/* CRT (syscon) */
#define CRT_PLL_SD1		0x1e0
#define CRT_PLL_SD2		0x1e4
#define CRT_PLL_SD3		0x1e8
#define CRT_PLL_SD4		0x1ec
#define PLL_SD2_VAL		0x04515893
#define PLL_SD3_100MHZ		0x00564388	/* vendor: "100 MHz" */
#define SD_CLK_BASE		50000000	/* card clock with no divider */

/* Pad drive block (realtek,pad-syscon = 0x98012620) */
#define PAD_PDRIVE_CR0		0x14
#define PAD_PDRIVE_CR1		0x18

#define RTD_SD_BUF_SIZE		SZ_128K
#define RTD_SD_CMD_TIMEOUT	msecs_to_jiffies(250)
#define RTD_SD_DATA_TIMEOUT	msecs_to_jiffies(3000)

struct rtd_sd_host {
	struct device *dev;
	struct mmc_host *mmc;
	void __iomem *base;
	struct regmap *crt;
	struct regmap *pad;		/* optional */
	struct regmap *sb2;		/* optional rbus sync */
	bool selected;			/* CMD7 done: 6.25 MHz floor */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_on;	/* optional "power-on" state */
	int irq;

	struct mutex lock;		/* one request at a time */
	struct mmc_request *mrq;
	struct work_struct req_work;
	struct completion done;
	u32 isr;

	void *buf;
	dma_addr_t buf_dma;
	unsigned int clock;
};

/* vendor rtk_sdmmc_sync(): order rbus accesses (SB2 0x9801a020) */
static void rtd_sd_sync(struct rtd_sd_host *h)
{
	wmb();
	if (h->sb2)
		regmap_write(h->sb2, 0x20, 0);
}

static inline u8 sd_rb(struct rtd_sd_host *h, u32 reg)
{
	return readb(h->base + reg);
}

static inline void sd_wb(struct rtd_sd_host *h, u32 reg, u8 val)
{
	writeb(val, h->base + reg);
}

static void sd_rmwb(struct rtd_sd_host *h, u32 reg, u8 mask, u8 val)
{
	sd_wb(h, reg, (sd_rb(h, reg) & ~mask) | val);
}

static void rtd_sd_pll_step(struct rtd_sd_host *h, u32 reg, u32 val, bool park)
{
	/* PLL_SD4 = 6 / 7 brackets every change, as in the vendor sequence */
	writel(DUMMY_PLL_CHANGE, h->base + SD_WRAP_DUMMY);
	udelay(100);
	regmap_write(h->crt, CRT_PLL_SD4, 0x6);
	if (park)
		writel(readl(h->base + SD_CKGEN_CTL) | CKGEN_SRC_4MHZ,
		       h->base + SD_CKGEN_CTL);
	else if (reg)
		regmap_write(h->crt, reg, val);
	else
		writel(readl(h->base + SD_CKGEN_CTL) & ~CKGEN_SRC_4MHZ,
		       h->base + SD_CKGEN_CTL);
	mdelay(2);
	regmap_write(h->crt, CRT_PLL_SD4, 0x7);
	writel(0, h->base + SD_WRAP_DUMMY);
}

/* Bring up PLL_SD at 100 MHz (50 MHz card clock) and the SD engine. */
static void rtd_sd_hw_init(struct rtd_sd_host *h)
{
	regmap_update_bits(h->crt, CRT_PLL_SD4, 0x7, 0x7);
	regmap_write(h->crt, CRT_PLL_SD1, 0x3);
	msleep(10);
	regmap_write(h->crt, CRT_PLL_SD1, 0x2003);

	rtd_sd_pll_step(h, 0, 0, true);		/* card clock from 4 MHz */
	rtd_sd_pll_step(h, CRT_PLL_SD2, PLL_SD2_VAL, false);
	rtd_sd_pll_step(h, CRT_PLL_SD3, PLL_SD3_100MHZ, false);

	/* disable L4 clock gating, reset the DMA */
	writel(readl(h->base + SD_WRAP_CTL) | BIT(1), h->base + SD_WRAP_CTL);
	sd_wb(h, SD_BUS_TA_STATE, 0x3);
	writel(readl(h->base + SD_WRAP_CTL) & ~BIT(0), h->base + SD_WRAP_CTL);
	writel(readl(h->base + SD_WRAP_CTL) | BIT(0), h->base + SD_WRAP_CTL);

	rtd_sd_pll_step(h, 0, 0, false);	/* card clock back to PLL */

	sd_rmwb(h, SD_CFG1, CFG1_FIFO_NOT_RST, 0);	/* reset FIFO */
	udelay(100);
	sd_wb(h, SD_CFG1, CFG1_DIV_256 | CFG1_FIFO_NOT_RST);
	writel(CKGEN_BASE, h->base + SD_CKGEN_CTL);

	writel(0, h->base + SD_PAD_CTL);		/* 3.3 V signalling */
	writel(0, h->base + SD_DMA_CTL3);

	writel(ISR_ALL, h->base + SD_ISR);		/* clear status */
	writel(ISR_ALL, h->base + SD_ISREN);		/* disable all */

	sd_wb(h, CARD_STOP, 0x00);
	sd_wb(h, CARD_SELECT, 0x02);			/* SD module */
	sd_wb(h, CARD_OE, 0x04);
	sd_wb(h, CARD_CLOCK_EN_CTL, CARD_CLK_EN_SD);
	sd_wb(h, SD_SAMPLE_POINT_CTL, 0x00);
	sd_wb(h, SD_PUSH_POINT_CTL, 0x00);
	sd_wb(h, SD_STATUS2, 0x00);

	if (h->pad) {
		regmap_write(h->pad, PAD_PDRIVE_CR0, 0x00003333);
		regmap_write(h->pad, PAD_PDRIVE_CR1, 0x33333333);
	}
}

static void rtd_sd_reset_engine(struct rtd_sd_host *h)
{
	writel(0, h->base + SD_DMA_CTL3);
	/* reset the DMA like the vendor driver does after a failure */
	writel(readl(h->base + SD_WRAP_CTL) & ~BIT(0), h->base + SD_WRAP_CTL);
	writel(readl(h->base + SD_WRAP_CTL) | BIT(0), h->base + SD_WRAP_CTL);
	writel(ISR_ALL, h->base + SD_ISREN);
	writel(ISR_ALL, h->base + SD_ISR);
	sd_wb(h, SD_TRANSFER, 0x00);
	sd_wb(h, CARD_STOP, 0xff);
	sd_wb(h, CARD_STOP, 0x00);
}

static irqreturn_t rtd_sd_irq(int irq, void *data)
{
	struct rtd_sd_host *h = data;
	u32 isr = readl(h->base + SD_ISR) & ISR_ALL;

	if (!isr)
		return IRQ_NONE;

	writel(isr, h->base + SD_ISR);		/* write-0 data bit: clear */
	h->isr |= isr;
	complete(&h->done);
	return IRQ_HANDLED;
}

/* Fire one SD_TRANSFER and wait for it; returns 0 or -errno. */
static int rtd_sd_run(struct rtd_sd_host *h, u8 tm, u32 irqs,
		      unsigned long timeout)
{
	u8 xfer, st2;
	int i;

	reinit_completion(&h->done);
	h->isr = 0;
	writel(ISR_ALL, h->base + SD_ISR);
	writel(ISR_ALL, h->base + SD_ISREN);
	writel(irqs | ISR_ERR | ISR_WRITE_DATA, h->base + SD_ISREN);

	rtd_sd_sync(h);
	sd_wb(h, SD_TRANSFER, tm | XFER_START);

	wait_for_completion_timeout(&h->done, timeout);
	rtd_sd_sync(h);

	/*
	 * The end state may trail the DMA-done interrupt; at 400 kHz by
	 * milliseconds (the vendor driver polls up to 300 jiffies).
	 */
	for (i = 0; i < 3000; i++) {
		rtd_sd_sync(h);
		xfer = sd_rb(h, SD_TRANSFER);
		if ((xfer & (XFER_END | XFER_IDLE)) == (XFER_END | XFER_IDLE) ||
		    (xfer & XFER_ERR))
			break;
		usleep_range(50, 100);
	}

	writel(ISR_ALL, h->base + SD_ISREN);

	if ((xfer & XFER_ERR) || (h->isr & ISR_ERR)) {
		st2 = sd_rb(h, SD_STATUS2);
		dev_dbg(h->dev, "tm %02x error: xfer %02x st1 %02x st2 %02x\n",
			tm, xfer, sd_rb(h, SD_STATUS1), st2);
		rtd_sd_reset_engine(h);
		return (st2 & STATUS2_RSP_TIMEOUT) ? -ETIMEDOUT : -EILSEQ;
	}

	if ((xfer & (XFER_END | XFER_IDLE)) != (XFER_END | XFER_IDLE)) {
		dev_dbg(h->dev, "tm %02x timeout: xfer %02x isr %x\n",
			tm, xfer, h->isr);
		rtd_sd_reset_engine(h);
		return -ETIMEDOUT;
	}

	return 0;
}

static void rtd_sd_setup_cmd(struct rtd_sd_host *h, struct mmc_command *cmd,
			     bool data)
{
	u8 cfg2, cfg3 = 0;

	if (!(cmd->flags & MMC_RSP_PRESENT))
		cfg2 = CFG2_RSP_NONE | CFG2_NO_CRC7_CHK;
	else if (cmd->flags & MMC_RSP_136)
		cfg2 = CFG2_RSP_17B;
	else
		cfg2 = CFG2_RSP_6B;

	if ((cmd->flags & MMC_RSP_PRESENT) && !(cmd->flags & MMC_RSP_CRC))
		cfg2 |= CFG2_NO_CRC7_CHK;
	if (!data) {
		cfg2 |= CFG2_NO_CRC16;
		if (cmd->flags & MMC_RSP_BUSY)
			cfg2 |= CFG2_WAIT_BUSY;
	}

	if (cmd->flags & MMC_RSP_PRESENT) {
		cfg3 = CFG3_RSP_TIMEOUT;
		if (cmd->flags & MMC_RSP_CRC)
			cfg3 |= CFG3_RSP_CHK;
	}

	sd_wb(h, SD_CFG2, cfg2);
	sd_wb(h, SD_CFG3, cfg3);

	sd_wb(h, SD_CMD0, 0x40 | cmd->opcode);
	sd_wb(h, SD_CMD0 + 1, cmd->arg >> 24);
	sd_wb(h, SD_CMD0 + 2, cmd->arg >> 16);
	sd_wb(h, SD_CMD0 + 3, cmd->arg >> 8);
	sd_wb(h, SD_CMD0 + 4, cmd->arg);
	sd_wb(h, SD_CMD5, 0);
}

static void rtd_sd_set_counts(struct rtd_sd_host *h, u16 bytes, u16 blocks)
{
	sd_wb(h, SD_BYTE_CNT_L, bytes);
	sd_wb(h, SD_BYTE_CNT_H, bytes >> 8);
	sd_wb(h, SD_BLOCK_CNT_L, blocks);
	sd_wb(h, SD_BLOCK_CNT_H, blocks >> 8);
}

static void rtd_sd_set_dma(struct rtd_sd_host *h, u32 blocks, u32 flags)
{
	writel(lower_32_bits(h->buf_dma) >> 3, h->base + SD_DMA_CTL1);
	writel(blocks, h->base + SD_DMA_CTL2);
	writel(flags | DMA_XFER, h->base + SD_DMA_CTL3);
}

static void rtd_sd_read_short_rsp(struct rtd_sd_host *h, struct mmc_command *cmd)
{
	cmd->resp[0] = sd_rb(h, SD_CMD0 + 1) << 24 | sd_rb(h, SD_CMD0 + 2) << 16 |
		       sd_rb(h, SD_CMD0 + 3) << 8 | sd_rb(h, SD_CMD0 + 4);
}

static int rtd_sd_send_cmd(struct rtd_sd_host *h, struct mmc_command *cmd)
{
	bool long_rsp = cmd->flags & MMC_RSP_136;
	u8 *b = h->buf;
	int ret;

	rtd_sd_setup_cmd(h, cmd, false);

	if (long_rsp) {
		/* the 17-byte R2 response is delivered by DMA */
		memset(h->buf, 0, 32);
		rtd_sd_set_counts(h, 0x200, 1);
		rtd_sd_set_dma(h, 1, DMA_RSP17_SEL | DMA_DDR_WR);
		ret = rtd_sd_run(h, TM_CMD_RSP, ISR_DMA_DONE, RTD_SD_CMD_TIMEOUT);
		writel(0, h->base + SD_DMA_CTL3);
	} else {
		ret = rtd_sd_run(h, TM_CMD_RSP, ISR_END,
				 (cmd->flags & MMC_RSP_BUSY) ?
				 RTD_SD_DATA_TIMEOUT : RTD_SD_CMD_TIMEOUT);
	}
	if (ret)
		return ret;

	if (long_rsp) {
		/* b[0] is the start/index byte; the last byte is in SD_CMD5 */
		cmd->resp[0] = get_unaligned_be32(b + 1);
		cmd->resp[1] = get_unaligned_be32(b + 5);
		cmd->resp[2] = get_unaligned_be32(b + 9);
		cmd->resp[3] = b[13] << 24 | b[14] << 16 | b[15] << 8 |
			       sd_rb(h, SD_CMD5);
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		rtd_sd_read_short_rsp(h, cmd);
	}
	return 0;
}

static int rtd_sd_data_cmd(struct rtd_sd_host *h, struct mmc_command *cmd)
{
	struct mmc_data *data = cmd->data;
	bool read = data->flags & MMC_DATA_READ;
	u32 len = data->blksz * data->blocks;
	u16 bytes = data->blksz, blocks = data->blocks;
	u32 dma_flags = read ? DMA_DDR_WR : 0;
	u8 tm;
	int ret;

	if (len > RTD_SD_BUF_SIZE)
		return -EINVAL;

	if (data->blksz < 512) {
		/*
		 * Short register reads (SD status, SWITCH, SCR, ...): the
		 * vendor driver always uses 64-byte DMA units here.
		 */
		bytes = 0x40;
		blocks = DIV_ROUND_UP(len, 0x40);
		dma_flags |= DMA_DAT64_SEL;
	}

	if (read)
		tm = (data->blksz < 512) ? TM_NORMALREAD : TM_AUTOREAD2;
	else
		tm = TM_AUTOWRITE2;

	if (!read)
		sg_copy_to_buffer(data->sg, data->sg_len, h->buf, len);

	rtd_sd_setup_cmd(h, cmd, true);
	/*
	 * Replies shorter than the 64-byte unit (SCR: 8 bytes, NUM_WR_BLKS:
	 * 4) cannot pass the CRC16 check over 64 bytes; the vendor driver
	 * disables it for those.
	 */
	if (data->blksz < 0x40)
		sd_rmwb(h, SD_CFG2, CFG2_NO_CRC16, CFG2_NO_CRC16);
	rtd_sd_set_counts(h, bytes, blocks);
	rtd_sd_set_dma(h, blocks, dma_flags);

	ret = rtd_sd_run(h, tm, read ? ISR_DMA_DONE : ISR_END,
			 RTD_SD_DATA_TIMEOUT);
	writel(0, h->base + SD_DMA_CTL3);
	if (ret) {
		data->error = ret;
		return ret;
	}

	rtd_sd_read_short_rsp(h, cmd);

	if (read)
		sg_copy_from_buffer(data->sg, data->sg_len, h->buf, len);
	data->bytes_xfered = len;
	return 0;
}

static void rtd_sd_set_clock(struct rtd_sd_host *h, unsigned int hz);

static void rtd_sd_req_work(struct work_struct *work)
{
	struct rtd_sd_host *h = container_of(work, struct rtd_sd_host, req_work);
	struct mmc_request *mrq = h->mrq;
	struct mmc_command *cmd = mrq->cmd;

	mutex_lock(&h->lock);

	if (mrq->sbc) {
		mrq->sbc->error = rtd_sd_send_cmd(h, mrq->sbc);
		if (mrq->sbc->error)
			goto done;
	}

	if (cmd->data)
		cmd->error = rtd_sd_data_cmd(h, cmd);
	else
		cmd->error = rtd_sd_send_cmd(h, cmd);

	if (cmd->opcode == MMC_GO_IDLE_STATE) {
		h->selected = false;
	} else if (cmd->opcode == MMC_SELECT_CARD && !cmd->error && cmd->arg &&
		   !h->selected) {
		h->selected = true;
		rtd_sd_set_clock(h, h->mmc->ios.clock);
	}

	/* open-ended multi-block transfer: send the stop ourselves */
	if (cmd->data && mrq->stop && !mrq->sbc &&
	    (cmd->error || cmd->data->blocks > 1 ||
	     cmd->opcode == MMC_READ_MULTIPLE_BLOCK ||
	     cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))
		mrq->stop->error = rtd_sd_send_cmd(h, mrq->stop);

done:
	h->mrq = NULL;
	mutex_unlock(&h->lock);
	mmc_request_done(h->mmc, mrq);
}

static void rtd_sd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct rtd_sd_host *h = mmc_priv(mmc);

	WARN_ON(h->mrq);
	h->mrq = mrq;
	schedule_work(&h->req_work);
}

/*
 * The vendor driver's table: a request of at least .hz uses SD_CFG1 card
 * divider .cfg and CKGEN engine divider 2^.ck, giving a card clock of
 * .actual.  The engine clock must stay undivided at the "400 kHz" step:
 * with /2 there the controller never receives the data phase.
 */
static const struct {
	unsigned int hz;
	unsigned int actual;
	u8 cfg;
	u8 ck;
} rtd_sd_clk_tbl[] = {
	{ 50000000, SD_CLK_BASE,           CFG1_DIV_1,   0 },
	{ 25000000, SD_CLK_BASE / 2,       CFG1_DIV_1,   1 },
	{  6250000, SD_CLK_BASE / 8,       CFG1_DIV_1,   3 },
	{   400000, SD_CLK_BASE / 256,     CFG1_DIV_256, 0 },
	{        0, SD_CLK_BASE / 256 / 2, CFG1_DIV_256, 1 },
};

static void rtd_sd_set_clock(struct rtd_sd_host *h, unsigned int hz)
{
	unsigned int req = hz;
	int i;

	/* clock 0 (power off/cycle): stop the card clock */
	if (!hz) {
		sd_rmwb(h, CARD_CLOCK_EN_CTL, CARD_CLK_EN_SD, 0);
		h->mmc->actual_clock = 0;
		h->clock = 0;
		return;
	}

	/*
	 * Like the vendor driver, never run data transfers with the card
	 * divider: once the card is selected, anything below 25 MHz runs at
	 * 6.25 MHz (with CFG1 /256 the controller misses the data phase).
	 */
	if (h->selected && hz < 6250000)
		hz = 6250000;

	for (i = 0; i < ARRAY_SIZE(rtd_sd_clk_tbl) - 1; i++)
		if (hz >= rtd_sd_clk_tbl[i].hz)
			break;

	sd_rmwb(h, SD_CFG1, CFG1_DIV_MASK | CFG1_MODE_MASK,
		rtd_sd_clk_tbl[i].cfg);
	writel(CKGEN_BASE | rtd_sd_clk_tbl[i].ck, h->base + SD_CKGEN_CTL);
	sd_rmwb(h, CARD_CLOCK_EN_CTL, CARD_CLK_EN_SD, CARD_CLK_EN_SD);
	h->mmc->actual_clock = rtd_sd_clk_tbl[i].actual;
	h->clock = req;		/* what the core asked for (set_ios compares) */
}

static void rtd_sd_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct rtd_sd_host *h = mmc_priv(mmc);

	mutex_lock(&h->lock);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		h->selected = false;
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, 0);
		if (h->pins_on)
			pinctrl_select_default_state(h->dev);
		break;
	case MMC_POWER_UP:
		if (h->pins_on)
			pinctrl_select_state(h->pinctrl, h->pins_on);
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, ios->vdd);
		break;
	default:
		break;
	}

	sd_rmwb(h, SD_CFG1, CFG1_BUS_MASK,
		ios->bus_width == MMC_BUS_WIDTH_4 ? CFG1_BUS_4 : 0);

	if (ios->clock != h->clock)
		rtd_sd_set_clock(h, ios->clock);

	mutex_unlock(&h->lock);
}

static int rtd_sd_get_cd(struct mmc_host *mmc)
{
	struct rtd_sd_host *h = mmc_priv(mmc);
	int gpio_cd = mmc_gpio_get_cd(mmc);

	if (gpio_cd >= 0)
		return gpio_cd;
	return !!(sd_rb(h, CARD_EXIST) & CARD_EXIST_SD);
}

static int rtd_sd_get_ro(struct mmc_host *mmc)
{
	struct rtd_sd_host *h = mmc_priv(mmc);
	int gpio_ro = mmc_gpio_get_ro(mmc);

	if (gpio_ro >= 0)
		return gpio_ro;
	return !!(sd_rb(h, CARD_EXIST) & CARD_EXIST_WP);
}

static int rtd_sd_card_busy(struct mmc_host *mmc)
{
	struct rtd_sd_host *h = mmc_priv(mmc);

	return !(sd_rb(h, SD_BUS_STATUS) & BUS_STATUS_DAT0);
}

static const struct mmc_host_ops rtd_sd_ops = {
	.request	= rtd_sd_request,
	.set_ios	= rtd_sd_set_ios,
	.get_cd		= rtd_sd_get_cd,
	.get_ro		= rtd_sd_get_ro,
	.card_busy	= rtd_sd_card_busy,
};

static int rtd_sd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_control *rst;
	struct mmc_host *mmc;
	struct rtd_sd_host *h;
	struct clk *clk;
	int ret;

	mmc = devm_mmc_alloc_host(dev, sizeof(*h));
	if (!mmc)
		return -ENOMEM;
	h = mmc_priv(mmc);
	h->dev = dev;
	h->mmc = mmc;
	mutex_init(&h->lock);
	init_completion(&h->done);
	INIT_WORK(&h->req_work, rtd_sd_req_work);

	h->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(h->base))
		return PTR_ERR(h->base);

	h->irq = platform_get_irq(pdev, 0);
	if (h->irq < 0)
		return h->irq;

	h->crt = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,crt");
	if (IS_ERR(h->crt))
		return dev_err_probe(dev, PTR_ERR(h->crt),
				     "missing realtek,crt syscon\n");
	h->sb2 = syscon_regmap_lookup_by_phandle_optional(dev->of_node,
							  "realtek,sb2");
	if (IS_ERR(h->sb2))
		return PTR_ERR(h->sb2);
	h->pad = syscon_regmap_lookup_by_phandle_optional(dev->of_node,
							  "realtek,pad-syscon");
	if (IS_ERR(h->pad))
		return PTR_ERR(h->pad);

	h->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(h->pinctrl)) {
		if (PTR_ERR(h->pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		h->pinctrl = NULL;
	} else {
		h->pins_on = pinctrl_lookup_state(h->pinctrl, "power-on");
		if (IS_ERR(h->pins_on))
			h->pins_on = NULL;
	}

	clk = devm_clk_get_optional_enabled(dev, "cr");
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "cr clock\n");
	clk = devm_clk_get_optional_enabled(dev, "sd_ip");
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "sd_ip clock\n");

	/*
	 * Never assert: the card-reader reset also covers the DMA bus shared
	 * with the eMMC controller.  Only make sure it is released.
	 */
	rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(rst))
		return dev_err_probe(dev, PTR_ERR(rst), "reset\n");
	ret = reset_control_deassert(rst);
	if (ret)
		return ret;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	h->buf = dmam_alloc_coherent(dev, RTD_SD_BUF_SIZE, &h->buf_dma,
				     GFP_KERNEL);
	if (!h->buf)
		return -ENOMEM;

	ret = mmc_regulator_get_supply(mmc);
	if (ret)
		return ret;

	ret = mmc_of_parse(mmc);
	if (ret)
		return ret;

	mmc->ops = &rtd_sd_ops;
	mmc->f_min = rtd_sd_clk_tbl[ARRAY_SIZE(rtd_sd_clk_tbl) - 1].actual;
	if (!mmc->f_max || mmc->f_max > SD_CLK_BASE)
		mmc->f_max = SD_CLK_BASE;
	if (!mmc->ocr_avail)
		mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED;
	if (!mmc_host_can_gpio_cd(mmc) && !(mmc->caps & MMC_CAP_NONREMOVABLE))
		mmc->caps |= MMC_CAP_NEEDS_POLL;
	mmc->caps2 |= MMC_CAP2_NO_SDIO | MMC_CAP2_NO_MMC;

	/* R1B waits RTD_SD_DATA_TIMEOUT; longer ones the core polls itself */
	mmc->max_busy_timeout = jiffies_to_msecs(RTD_SD_DATA_TIMEOUT);
	mmc->max_segs = 128;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = RTD_SD_BUF_SIZE / 512;
	mmc->max_req_size = RTD_SD_BUF_SIZE;
	mmc->max_seg_size = RTD_SD_BUF_SIZE;

	rtd_sd_hw_init(h);
	rtd_sd_set_clock(h, mmc->f_min);

	ret = devm_request_irq(dev, h->irq, rtd_sd_irq, 0, dev_name(dev), h);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, h);
	return mmc_add_host(mmc);
}

static void rtd_sd_remove(struct platform_device *pdev)
{
	struct rtd_sd_host *h = platform_get_drvdata(pdev);

	mmc_remove_host(h->mmc);
	cancel_work_sync(&h->req_work);
	writel(ISR_ALL, h->base + SD_ISREN);
}

static const struct of_device_id rtd_sd_of_match[] = {
	{ .compatible = "realtek,rtd1295-sd" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd_sd_of_match);

static struct platform_driver rtd_sd_driver = {
	.probe	= rtd_sd_probe,
	.remove	= rtd_sd_remove,
	.driver	= {
		.name		= "rtd1295-sd",
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table	= rtd_sd_of_match,
	},
};
module_platform_driver(rtd_sd_driver);

MODULE_DESCRIPTION("Realtek RTD1295 SD card host driver");
MODULE_LICENSE("GPL");
