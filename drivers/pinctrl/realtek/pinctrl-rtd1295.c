// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek RTD1293/RTD1295/RTD1296 pin controller data
 *
 * The RTD129x pad mux and pad configuration registers are spread over four
 * register blocks, each a separate instance of the Realtek DHC pinctrl core:
 *
 *   iso   0x98007300 (ISO pads, MISC GPIO numbering "iso_gpio_N")
 *   sb2   0x9801a900 (MISC GPIO 0..53: SPI, GSPI, TP, I2C4/5, RGMII)
 *   disp  0x9804d000 (MISC GPIO 54..63: SPDIF, DMIC, audio out)
 *   cr    0x98012600 (MISC GPIO 64..100: NAND/eMMC, SD card, SDIO, PCIe)
 *
 * Pad configuration (per pad, 4 bits): bit 0 pull select (1 = up),
 * bit 1 pull enable, bit 2 drive strength, bit 3 schmitt trigger.
 *
 * Data derived from the Realtek 4.9 BSP pinctrl-rtd129x.h.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-rtd.h"

#define DECLARE_RTD1295_PIN(_inst, _pin, _name) \
	static const unsigned int rtd1295_##_inst##_##_name##_pins[] = { _pin }

#define RTD1295_GROUP(_inst, _name) \
	{ \
		.name = #_name, \
		.pins = rtd1295_##_inst##_##_name##_pins, \
		.num_pins = ARRAY_SIZE(rtd1295_##_inst##_##_name##_pins), \
	}

#define RTD1295_FUNC(_inst, _name) \
	{ \
		.name = #_name, \
		.groups = rtd1295_##_inst##_##_name##_groups, \
		.num_groups = ARRAY_SIZE(rtd1295_##_inst##_##_name##_groups), \
	}

enum rtd1295_sb2_pins {
	RTD1295_SB2_GPIO_0 = 0,
	RTD1295_SB2_GPIO_1,
	RTD1295_SB2_GPIO_2,
	RTD1295_SB2_GPIO_3,
	RTD1295_SB2_GPIO_4,
	RTD1295_SB2_GPIO_5,
	RTD1295_SB2_GPIO_6,
	RTD1295_SB2_GPIO_7,
	RTD1295_SB2_GPIO_8,
	RTD1295_SB2_GPIO_9,
	RTD1295_SB2_TP1_SYNC,
	RTD1295_SB2_I2C_SCL_4,
	RTD1295_SB2_I2C_SDA_4,
	RTD1295_SB2_I2C_SCL_5,
	RTD1295_SB2_I2C_SDA_5,
	RTD1295_SB2_USB_ID,
	RTD1295_SB2_SENSOR_CKO_0,
	RTD1295_SB2_SENSOR_CKO_1,
	RTD1295_SB2_SENSOR_RST,
	RTD1295_SB2_SENSOR_STB_0,
	RTD1295_SB2_SENSOR_STB_1,
	RTD1295_SB2_TP0_DATA,
	RTD1295_SB2_TP0_CLK,
	RTD1295_SB2_TP0_VALID,
	RTD1295_SB2_TP0_SYNC,
	RTD1295_SB2_TP1_DATA,
	RTD1295_SB2_TP1_CLK,
	RTD1295_SB2_TP1_VALID,
	RTD1295_SB2_RGMII0_TXC,
	RTD1295_SB2_RGMII0_TX_CTL,
	RTD1295_SB2_RGMII0_TXD_0,
	RTD1295_SB2_RGMII0_TXD_1,
	RTD1295_SB2_RGMII0_TXD_2,
	RTD1295_SB2_RGMII0_TXD_3,
	RTD1295_SB2_RGMII0_RXC,
	RTD1295_SB2_RGMII0_RX_CTL,
	RTD1295_SB2_RGMII0_RXD_0,
	RTD1295_SB2_RGMII0_RXD_1,
	RTD1295_SB2_RGMII0_RXD_2,
	RTD1295_SB2_RGMII0_RXD_3,
	RTD1295_SB2_RGMII0_MDIO,
	RTD1295_SB2_RGMII0_MDC,
	RTD1295_SB2_RGMII1_TXC,
	RTD1295_SB2_RGMII1_TX_CTL,
	RTD1295_SB2_RGMII1_TXD_0,
	RTD1295_SB2_RGMII1_TXD_1,
	RTD1295_SB2_RGMII1_TXD_2,
	RTD1295_SB2_RGMII1_TXD_3,
	RTD1295_SB2_RGMII1_RXC,
	RTD1295_SB2_RGMII1_RX_CTL,
	RTD1295_SB2_RGMII1_RXD_0,
	RTD1295_SB2_RGMII1_RXD_1,
	RTD1295_SB2_RGMII1_RXD_2,
	RTD1295_SB2_RGMII1_RXD_3,
	RTD1295_SB2_HI_LOC,
	RTD1295_SB2_EJTAG_SCPU_LOC,
	RTD1295_SB2_SF_EN,
	RTD1295_SB2_ARM_TRACE_DBG_EN,
	RTD1295_SB2_DEBUG_P2S_ENABLE,
	RTD1295_SB2_TP0_LOC,
	RTD1295_SB2_TP1_LOC,
	RTD1295_SB2_NUM_PINS
};

static const struct pinctrl_pin_desc rtd1295_sb2_pins[] = {
	PINCTRL_PIN(RTD1295_SB2_GPIO_0, "gpio_0"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_1, "gpio_1"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_2, "gpio_2"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_3, "gpio_3"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_4, "gpio_4"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_5, "gpio_5"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_6, "gpio_6"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_7, "gpio_7"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_8, "gpio_8"),
	PINCTRL_PIN(RTD1295_SB2_GPIO_9, "gpio_9"),
	PINCTRL_PIN(RTD1295_SB2_TP1_SYNC, "tp1_sync"),
	PINCTRL_PIN(RTD1295_SB2_I2C_SCL_4, "i2c_scl_4"),
	PINCTRL_PIN(RTD1295_SB2_I2C_SDA_4, "i2c_sda_4"),
	PINCTRL_PIN(RTD1295_SB2_I2C_SCL_5, "i2c_scl_5"),
	PINCTRL_PIN(RTD1295_SB2_I2C_SDA_5, "i2c_sda_5"),
	PINCTRL_PIN(RTD1295_SB2_USB_ID, "usb_id"),
	PINCTRL_PIN(RTD1295_SB2_SENSOR_CKO_0, "sensor_cko_0"),
	PINCTRL_PIN(RTD1295_SB2_SENSOR_CKO_1, "sensor_cko_1"),
	PINCTRL_PIN(RTD1295_SB2_SENSOR_RST, "sensor_rst"),
	PINCTRL_PIN(RTD1295_SB2_SENSOR_STB_0, "sensor_stb_0"),
	PINCTRL_PIN(RTD1295_SB2_SENSOR_STB_1, "sensor_stb_1"),
	PINCTRL_PIN(RTD1295_SB2_TP0_DATA, "tp0_data"),
	PINCTRL_PIN(RTD1295_SB2_TP0_CLK, "tp0_clk"),
	PINCTRL_PIN(RTD1295_SB2_TP0_VALID, "tp0_valid"),
	PINCTRL_PIN(RTD1295_SB2_TP0_SYNC, "tp0_sync"),
	PINCTRL_PIN(RTD1295_SB2_TP1_DATA, "tp1_data"),
	PINCTRL_PIN(RTD1295_SB2_TP1_CLK, "tp1_clk"),
	PINCTRL_PIN(RTD1295_SB2_TP1_VALID, "tp1_valid"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TXC, "rgmii0_txc"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TX_CTL, "rgmii0_tx_ctl"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TXD_0, "rgmii0_txd_0"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TXD_1, "rgmii0_txd_1"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TXD_2, "rgmii0_txd_2"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_TXD_3, "rgmii0_txd_3"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RXC, "rgmii0_rxc"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RX_CTL, "rgmii0_rx_ctl"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RXD_0, "rgmii0_rxd_0"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RXD_1, "rgmii0_rxd_1"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RXD_2, "rgmii0_rxd_2"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_RXD_3, "rgmii0_rxd_3"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_MDIO, "rgmii0_mdio"),
	PINCTRL_PIN(RTD1295_SB2_RGMII0_MDC, "rgmii0_mdc"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TXC, "rgmii1_txc"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TX_CTL, "rgmii1_tx_ctl"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TXD_0, "rgmii1_txd_0"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TXD_1, "rgmii1_txd_1"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TXD_2, "rgmii1_txd_2"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_TXD_3, "rgmii1_txd_3"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RXC, "rgmii1_rxc"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RX_CTL, "rgmii1_rx_ctl"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RXD_0, "rgmii1_rxd_0"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RXD_1, "rgmii1_rxd_1"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RXD_2, "rgmii1_rxd_2"),
	PINCTRL_PIN(RTD1295_SB2_RGMII1_RXD_3, "rgmii1_rxd_3"),
	PINCTRL_PIN(RTD1295_SB2_HI_LOC, "hi_loc"),
	PINCTRL_PIN(RTD1295_SB2_EJTAG_SCPU_LOC, "ejtag_scpu_loc"),
	PINCTRL_PIN(RTD1295_SB2_SF_EN, "sf_en"),
	PINCTRL_PIN(RTD1295_SB2_ARM_TRACE_DBG_EN, "arm_trace_dbg_en"),
	PINCTRL_PIN(RTD1295_SB2_DEBUG_P2S_ENABLE, "debug_p2s_enable"),
	PINCTRL_PIN(RTD1295_SB2_TP0_LOC, "tp0_loc"),
	PINCTRL_PIN(RTD1295_SB2_TP1_LOC, "tp1_loc"),
};

DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_0, gpio_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_1, gpio_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_2, gpio_2);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_3, gpio_3);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_4, gpio_4);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_5, gpio_5);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_6, gpio_6);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_7, gpio_7);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_8, gpio_8);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_GPIO_9, gpio_9);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP1_SYNC, tp1_sync);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_I2C_SCL_4, i2c_scl_4);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_I2C_SDA_4, i2c_sda_4);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_I2C_SCL_5, i2c_scl_5);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_I2C_SDA_5, i2c_sda_5);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_USB_ID, usb_id);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SENSOR_CKO_0, sensor_cko_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SENSOR_CKO_1, sensor_cko_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SENSOR_RST, sensor_rst);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SENSOR_STB_0, sensor_stb_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SENSOR_STB_1, sensor_stb_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP0_DATA, tp0_data);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP0_CLK, tp0_clk);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP0_VALID, tp0_valid);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP0_SYNC, tp0_sync);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP1_DATA, tp1_data);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP1_CLK, tp1_clk);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP1_VALID, tp1_valid);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TXC, rgmii0_txc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TX_CTL, rgmii0_tx_ctl);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TXD_0, rgmii0_txd_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TXD_1, rgmii0_txd_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TXD_2, rgmii0_txd_2);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_TXD_3, rgmii0_txd_3);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RXC, rgmii0_rxc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RX_CTL, rgmii0_rx_ctl);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RXD_0, rgmii0_rxd_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RXD_1, rgmii0_rxd_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RXD_2, rgmii0_rxd_2);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_RXD_3, rgmii0_rxd_3);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_MDIO, rgmii0_mdio);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII0_MDC, rgmii0_mdc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TXC, rgmii1_txc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TX_CTL, rgmii1_tx_ctl);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TXD_0, rgmii1_txd_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TXD_1, rgmii1_txd_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TXD_2, rgmii1_txd_2);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_TXD_3, rgmii1_txd_3);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RXC, rgmii1_rxc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RX_CTL, rgmii1_rx_ctl);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RXD_0, rgmii1_rxd_0);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RXD_1, rgmii1_rxd_1);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RXD_2, rgmii1_rxd_2);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_RGMII1_RXD_3, rgmii1_rxd_3);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_HI_LOC, hi_loc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_EJTAG_SCPU_LOC, ejtag_scpu_loc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_SF_EN, sf_en);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_ARM_TRACE_DBG_EN, arm_trace_dbg_en);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_DEBUG_P2S_ENABLE, debug_p2s_enable);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP0_LOC, tp0_loc);
DECLARE_RTD1295_PIN(sb2, RTD1295_SB2_TP1_LOC, tp1_loc);

static const struct rtd_pin_group_desc rtd1295_sb2_pin_groups[] = {
	RTD1295_GROUP(sb2, gpio_0),
	RTD1295_GROUP(sb2, gpio_1),
	RTD1295_GROUP(sb2, gpio_2),
	RTD1295_GROUP(sb2, gpio_3),
	RTD1295_GROUP(sb2, gpio_4),
	RTD1295_GROUP(sb2, gpio_5),
	RTD1295_GROUP(sb2, gpio_6),
	RTD1295_GROUP(sb2, gpio_7),
	RTD1295_GROUP(sb2, gpio_8),
	RTD1295_GROUP(sb2, gpio_9),
	RTD1295_GROUP(sb2, tp1_sync),
	RTD1295_GROUP(sb2, i2c_scl_4),
	RTD1295_GROUP(sb2, i2c_sda_4),
	RTD1295_GROUP(sb2, i2c_scl_5),
	RTD1295_GROUP(sb2, i2c_sda_5),
	RTD1295_GROUP(sb2, usb_id),
	RTD1295_GROUP(sb2, sensor_cko_0),
	RTD1295_GROUP(sb2, sensor_cko_1),
	RTD1295_GROUP(sb2, sensor_rst),
	RTD1295_GROUP(sb2, sensor_stb_0),
	RTD1295_GROUP(sb2, sensor_stb_1),
	RTD1295_GROUP(sb2, tp0_data),
	RTD1295_GROUP(sb2, tp0_clk),
	RTD1295_GROUP(sb2, tp0_valid),
	RTD1295_GROUP(sb2, tp0_sync),
	RTD1295_GROUP(sb2, tp1_data),
	RTD1295_GROUP(sb2, tp1_clk),
	RTD1295_GROUP(sb2, tp1_valid),
	RTD1295_GROUP(sb2, rgmii0_txc),
	RTD1295_GROUP(sb2, rgmii0_tx_ctl),
	RTD1295_GROUP(sb2, rgmii0_txd_0),
	RTD1295_GROUP(sb2, rgmii0_txd_1),
	RTD1295_GROUP(sb2, rgmii0_txd_2),
	RTD1295_GROUP(sb2, rgmii0_txd_3),
	RTD1295_GROUP(sb2, rgmii0_rxc),
	RTD1295_GROUP(sb2, rgmii0_rx_ctl),
	RTD1295_GROUP(sb2, rgmii0_rxd_0),
	RTD1295_GROUP(sb2, rgmii0_rxd_1),
	RTD1295_GROUP(sb2, rgmii0_rxd_2),
	RTD1295_GROUP(sb2, rgmii0_rxd_3),
	RTD1295_GROUP(sb2, rgmii0_mdio),
	RTD1295_GROUP(sb2, rgmii0_mdc),
	RTD1295_GROUP(sb2, rgmii1_txc),
	RTD1295_GROUP(sb2, rgmii1_tx_ctl),
	RTD1295_GROUP(sb2, rgmii1_txd_0),
	RTD1295_GROUP(sb2, rgmii1_txd_1),
	RTD1295_GROUP(sb2, rgmii1_txd_2),
	RTD1295_GROUP(sb2, rgmii1_txd_3),
	RTD1295_GROUP(sb2, rgmii1_rxc),
	RTD1295_GROUP(sb2, rgmii1_rx_ctl),
	RTD1295_GROUP(sb2, rgmii1_rxd_0),
	RTD1295_GROUP(sb2, rgmii1_rxd_1),
	RTD1295_GROUP(sb2, rgmii1_rxd_2),
	RTD1295_GROUP(sb2, rgmii1_rxd_3),
	RTD1295_GROUP(sb2, hi_loc),
	RTD1295_GROUP(sb2, ejtag_scpu_loc),
	RTD1295_GROUP(sb2, sf_en),
	RTD1295_GROUP(sb2, arm_trace_dbg_en),
	RTD1295_GROUP(sb2, debug_p2s_enable),
	RTD1295_GROUP(sb2, tp0_loc),
	RTD1295_GROUP(sb2, tp1_loc),
};

static const char * const rtd1295_sb2_gpio_groups[] = {
	"gpio_0", "gpio_1", "gpio_2", "gpio_3", "gpio_4", "gpio_5", "gpio_6",
	"gpio_7", "gpio_8", "gpio_9", "tp1_sync", "i2c_scl_4", "i2c_sda_4",
	"i2c_scl_5", "i2c_sda_5", "usb_id", "sensor_cko_0", "sensor_cko_1",
	"sensor_rst", "sensor_stb_0", "sensor_stb_1", "tp0_data", "tp0_clk",
	"tp0_valid", "tp0_sync", "tp1_data", "tp1_clk", "tp1_valid",
	"rgmii0_txc", "rgmii0_tx_ctl", "rgmii0_txd_0", "rgmii0_txd_1",
	"rgmii0_txd_2", "rgmii0_txd_3", "rgmii0_rxc", "rgmii0_rx_ctl",
	"rgmii0_rxd_0", "rgmii0_rxd_1", "rgmii0_rxd_2", "rgmii0_rxd_3",
	"rgmii0_mdio", "rgmii0_mdc", "rgmii1_txc", "rgmii1_tx_ctl",
	"rgmii1_txd_0", "rgmii1_txd_1", "rgmii1_txd_2", "rgmii1_txd_3",
	"rgmii1_rxc", "rgmii1_rx_ctl", "rgmii1_rxd_0", "rgmii1_rxd_1",
	"rgmii1_rxd_2", "rgmii1_rxd_3", "sf_en"
};
static const char * const rtd1295_sb2_ai_groups[] = {
	"tp0_data", "tp0_clk", "tp0_valid", "tp0_sync"
};
static const char * const rtd1295_sb2_arm_trace_disable_groups[] = { "arm_trace_dbg_en" };
static const char * const rtd1295_sb2_arm_trace_enable_groups[] = { "arm_trace_dbg_en" };
static const char * const rtd1295_sb2_dc_fan_sensor_groups[] = { "gpio_9" };
static const char * const rtd1295_sb2_eth_gpy_groups[] = { "rgmii0_mdio", "rgmii0_mdc" };
static const char * const rtd1295_sb2_gspi_groups[] = { "gpio_4", "gpio_5", "gpio_6", "gpio_7" };
static const char * const rtd1295_sb2_hi_loc_misc_groups[] = { "hi_loc" };
static const char * const rtd1295_sb2_hi_loc_nf_groups[] = { "hi_loc" };
static const char * const rtd1295_sb2_i2c2_groups[] = { "tp1_sync", "tp1_clk" };
static const char * const rtd1295_sb2_i2c3_groups[] = { "tp1_data", "tp1_valid" };
static const char * const rtd1295_sb2_i2c4_groups[] = { "i2c_scl_4", "i2c_sda_4" };
static const char * const rtd1295_sb2_i2c5_groups[] = { "i2c_scl_5", "i2c_sda_5" };
static const char * const rtd1295_sb2_nand_groups[] = { "i2c_scl_5", "i2c_sda_5" };
static const char * const rtd1295_sb2_p2s_disable_groups[] = { "debug_p2s_enable" };
static const char * const rtd1295_sb2_p2s_enable_groups[] = { "debug_p2s_enable" };
static const char * const rtd1295_sb2_rgmii_groups[] = {
	"rgmii0_txc", "rgmii0_tx_ctl", "rgmii0_txd_0", "rgmii0_txd_1",
	"rgmii0_txd_2", "rgmii0_txd_3", "rgmii0_rxc", "rgmii0_rx_ctl",
	"rgmii0_rxd_0", "rgmii0_rxd_1", "rgmii0_rxd_2", "rgmii0_rxd_3",
	"rgmii0_mdio", "rgmii0_mdc", "rgmii1_txc", "rgmii1_tx_ctl",
	"rgmii1_txd_0", "rgmii1_txd_1", "rgmii1_txd_2", "rgmii1_txd_3",
	"rgmii1_rxc", "rgmii1_rx_ctl", "rgmii1_rxd_0", "rgmii1_rxd_1",
	"rgmii1_rxd_2", "rgmii1_rxd_3"
};
static const char * const rtd1295_sb2_scpu_ejtag_loc_cr_groups[] = { "ejtag_scpu_loc" };
static const char * const rtd1295_sb2_scpu_ejtag_loc_gpio_groups[] = {
	"gpio_4", "gpio_5", "gpio_6", "gpio_7", "gpio_8", "ejtag_scpu_loc"
};
static const char * const rtd1295_sb2_sensor_cko_output_groups[] = {
	"sensor_cko_0", "sensor_cko_1"
};
static const char * const rtd1295_sb2_spi_groups[] = {
	"gpio_0", "gpio_1", "gpio_2", "gpio_3", "sf_en"
};
static const char * const rtd1295_sb2_test_loop_dis_groups[] = { "usb_id" };
static const char * const rtd1295_sb2_tp0_loc_rgmii0_tx_groups[] = {
	"rgmii0_txd_0", "rgmii0_txd_1", "rgmii0_txd_2", "rgmii0_txd_3",
	"tp0_loc"
};
static const char * const rtd1295_sb2_tp0_loc_tp0_groups[] = {
	"tp0_data", "tp0_clk", "tp0_valid", "tp0_sync", "tp0_loc"
};
static const char * const rtd1295_sb2_tp0_loc_tp1_groups[] = {
	"tp0_data", "tp0_clk", "tp0_valid", "tp0_sync", "tp0_loc"
};
static const char * const rtd1295_sb2_tp1_loc_rgmii0_rx_groups[] = {
	"rgmii0_rxd_0", "rgmii0_rxd_1", "rgmii0_rxd_2", "rgmii0_rxd_3",
	"tp1_loc"
};
static const char * const rtd1295_sb2_tp1_loc_tp0_groups[] = {
	"tp1_sync", "tp1_data", "tp1_clk", "tp1_valid", "tp1_loc"
};
static const char * const rtd1295_sb2_tp1_loc_tp1_groups[] = {
	"tp1_sync", "tp1_data", "tp1_clk", "tp1_valid", "tp1_loc"
};
static const char * const rtd1295_sb2_usb_clock_output_groups[] = { "sensor_cko_1" };

static const struct rtd_pin_func_desc rtd1295_sb2_pin_functions[] = {
	RTD1295_FUNC(sb2, gpio),
	RTD1295_FUNC(sb2, ai),
	RTD1295_FUNC(sb2, arm_trace_disable),
	RTD1295_FUNC(sb2, arm_trace_enable),
	RTD1295_FUNC(sb2, dc_fan_sensor),
	RTD1295_FUNC(sb2, eth_gpy),
	RTD1295_FUNC(sb2, gspi),
	RTD1295_FUNC(sb2, hi_loc_misc),
	RTD1295_FUNC(sb2, hi_loc_nf),
	RTD1295_FUNC(sb2, i2c2),
	RTD1295_FUNC(sb2, i2c3),
	RTD1295_FUNC(sb2, i2c4),
	RTD1295_FUNC(sb2, i2c5),
	RTD1295_FUNC(sb2, nand),
	RTD1295_FUNC(sb2, p2s_disable),
	RTD1295_FUNC(sb2, p2s_enable),
	RTD1295_FUNC(sb2, rgmii),
	RTD1295_FUNC(sb2, scpu_ejtag_loc_cr),
	RTD1295_FUNC(sb2, scpu_ejtag_loc_gpio),
	RTD1295_FUNC(sb2, sensor_cko_output),
	RTD1295_FUNC(sb2, spi),
	RTD1295_FUNC(sb2, test_loop_dis),
	RTD1295_FUNC(sb2, tp0_loc_rgmii0_tx),
	RTD1295_FUNC(sb2, tp0_loc_tp0),
	RTD1295_FUNC(sb2, tp0_loc_tp1),
	RTD1295_FUNC(sb2, tp1_loc_rgmii0_rx),
	RTD1295_FUNC(sb2, tp1_loc_tp0),
	RTD1295_FUNC(sb2, tp1_loc_tp1),
	RTD1295_FUNC(sb2, usb_clock_output),
};

static const struct rtd_pin_desc rtd1295_sb2_muxes[RTD1295_SB2_NUM_PINS] = {
	[RTD1295_SB2_GPIO_0] = RTK_PIN_MUX(gpio_0, 0x10, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "spi")),
	[RTD1295_SB2_GPIO_1] = RTK_PIN_MUX(gpio_1, 0x10, GENMASK(5, 3),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 3), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 3), "spi")),
	[RTD1295_SB2_GPIO_2] = RTK_PIN_MUX(gpio_2, 0x10, GENMASK(8, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 6), "spi")),
	[RTD1295_SB2_GPIO_3] = RTK_PIN_MUX(gpio_3, 0x10, GENMASK(11, 9),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 9), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 9), "spi")),
	[RTD1295_SB2_GPIO_4] = RTK_PIN_MUX(gpio_4, 0x10, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "scpu_ejtag_loc_gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "gspi")),
	[RTD1295_SB2_GPIO_5] = RTK_PIN_MUX(gpio_5, 0x10, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 14), "scpu_ejtag_loc_gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 14), "gspi")),
	[RTD1295_SB2_GPIO_6] = RTK_PIN_MUX(gpio_6, 0x10, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "scpu_ejtag_loc_gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "gspi")),
	[RTD1295_SB2_GPIO_7] = RTK_PIN_MUX(gpio_7, 0x10, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "scpu_ejtag_loc_gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 18), "gspi")),
	[RTD1295_SB2_GPIO_8] = RTK_PIN_MUX(gpio_8, 0x10, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "scpu_ejtag_loc_gpio")),
	[RTD1295_SB2_GPIO_9] = RTK_PIN_MUX(gpio_9, 0x10, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 22), "dc_fan_sensor")),
	[RTD1295_SB2_TP1_SYNC] = RTK_PIN_MUX(tp1_sync, 0x8, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "tp1_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "tp1_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 18), "i2c2")),
	[RTD1295_SB2_I2C_SCL_4] = RTK_PIN_MUX(i2c_scl_4, 0xc, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "i2c4")),
	[RTD1295_SB2_I2C_SDA_4] = RTK_PIN_MUX(i2c_sda_4, 0xc, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "i2c4")),
	[RTD1295_SB2_I2C_SCL_5] = RTK_PIN_MUX(i2c_scl_5, 0xc, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "i2c5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 10), "nand")),
	[RTD1295_SB2_I2C_SDA_5] = RTK_PIN_MUX(i2c_sda_5, 0xc, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "i2c5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "nand")),
	[RTD1295_SB2_USB_ID] = RTK_PIN_MUX(usb_id, 0xc, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "test_loop_dis")),
	[RTD1295_SB2_SENSOR_CKO_0] = RTK_PIN_MUX(sensor_cko_0, 0xc, GENMASK(31, 30),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 30), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 30), "sensor_cko_output")),
	[RTD1295_SB2_SENSOR_CKO_1] = RTK_PIN_MUX(sensor_cko_1, 0xc, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "sensor_cko_output"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 28), "usb_clock_output")),
	[RTD1295_SB2_SENSOR_RST] = RTK_PIN_MUX(sensor_rst, 0xc, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "gpio")),
	[RTD1295_SB2_SENSOR_STB_0] = RTK_PIN_MUX(sensor_stb_0, 0xc, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio")),
	[RTD1295_SB2_SENSOR_STB_1] = RTK_PIN_MUX(sensor_stb_1, 0xc, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio")),
	[RTD1295_SB2_TP0_DATA] = RTK_PIN_MUX(tp0_data, 0x8, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "tp0_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "tp0_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "ai")),
	[RTD1295_SB2_TP0_CLK] = RTK_PIN_MUX(tp0_clk, 0x8, GENMASK(11, 9),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 9), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 9), "tp0_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 9), "tp0_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 9), "ai")),
	[RTD1295_SB2_TP0_VALID] = RTK_PIN_MUX(tp0_valid, 0x8, GENMASK(8, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "tp0_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 6), "tp0_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 6), "ai")),
	[RTD1295_SB2_TP0_SYNC] = RTK_PIN_MUX(tp0_sync, 0x8, GENMASK(5, 3),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 3), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 3), "tp0_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 3), "tp0_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 3), "ai")),
	[RTD1295_SB2_TP1_DATA] = RTK_PIN_MUX(tp1_data, 0x8, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "tp1_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "tp1_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "i2c3")),
	[RTD1295_SB2_TP1_CLK] = RTK_PIN_MUX(tp1_clk, 0x8, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "tp1_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 22), "tp1_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 22), "i2c2")),
	[RTD1295_SB2_TP1_VALID] = RTK_PIN_MUX(tp1_valid, 0x8, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "tp1_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "tp1_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "i2c3")),
	[RTD1295_SB2_RGMII0_TXC] = RTK_PIN_MUX(rgmii0_txc, 0x6c, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "rgmii")),
	[RTD1295_SB2_RGMII0_TX_CTL] = RTK_PIN_MUX(rgmii0_tx_ctl, 0x6c, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "rgmii")),
	[RTD1295_SB2_RGMII0_TXD_0] = RTK_PIN_MUX(rgmii0_txd_0, 0x6c, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "tp0_loc_rgmii0_tx")),
	[RTD1295_SB2_RGMII0_TXD_1] = RTK_PIN_MUX(rgmii0_txd_1, 0x6c, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 6), "tp0_loc_rgmii0_tx")),
	[RTD1295_SB2_RGMII0_TXD_2] = RTK_PIN_MUX(rgmii0_txd_2, 0x6c, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "tp0_loc_rgmii0_tx")),
	[RTD1295_SB2_RGMII0_TXD_3] = RTK_PIN_MUX(rgmii0_txd_3, 0x6c, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 10), "tp0_loc_rgmii0_tx")),
	[RTD1295_SB2_RGMII0_RXC] = RTK_PIN_MUX(rgmii0_rxc, 0x6c, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "rgmii")),
	[RTD1295_SB2_RGMII0_RX_CTL] = RTK_PIN_MUX(rgmii0_rx_ctl, 0x6c, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "rgmii")),
	[RTD1295_SB2_RGMII0_RXD_0] = RTK_PIN_MUX(rgmii0_rxd_0, 0x6c, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "tp1_loc_rgmii0_rx")),
	[RTD1295_SB2_RGMII0_RXD_1] = RTK_PIN_MUX(rgmii0_rxd_1, 0x6c, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "tp1_loc_rgmii0_rx")),
	[RTD1295_SB2_RGMII0_RXD_2] = RTK_PIN_MUX(rgmii0_rxd_2, 0x6c, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "tp1_loc_rgmii0_rx")),
	[RTD1295_SB2_RGMII0_RXD_3] = RTK_PIN_MUX(rgmii0_rxd_3, 0x6c, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 22), "tp1_loc_rgmii0_rx")),
	[RTD1295_SB2_RGMII0_MDIO] = RTK_PIN_MUX(rgmii0_mdio, 0x6c, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "eth_gpy")),
	[RTD1295_SB2_RGMII0_MDC] = RTK_PIN_MUX(rgmii0_mdc, 0x6c, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "rgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "eth_gpy")),
	[RTD1295_SB2_RGMII1_TXC] = RTK_PIN_MUX(rgmii1_txc, 0x7c, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "rgmii")),
	[RTD1295_SB2_RGMII1_TX_CTL] = RTK_PIN_MUX(rgmii1_tx_ctl, 0x7c, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "rgmii")),
	[RTD1295_SB2_RGMII1_TXD_0] = RTK_PIN_MUX(rgmii1_txd_0, 0x7c, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "rgmii")),
	[RTD1295_SB2_RGMII1_TXD_1] = RTK_PIN_MUX(rgmii1_txd_1, 0x7c, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "rgmii")),
	[RTD1295_SB2_RGMII1_TXD_2] = RTK_PIN_MUX(rgmii1_txd_2, 0x7c, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "rgmii")),
	[RTD1295_SB2_RGMII1_TXD_3] = RTK_PIN_MUX(rgmii1_txd_3, 0x7c, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "rgmii")),
	[RTD1295_SB2_RGMII1_RXC] = RTK_PIN_MUX(rgmii1_rxc, 0x7c, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "rgmii")),
	[RTD1295_SB2_RGMII1_RX_CTL] = RTK_PIN_MUX(rgmii1_rx_ctl, 0x7c, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "rgmii")),
	[RTD1295_SB2_RGMII1_RXD_0] = RTK_PIN_MUX(rgmii1_rxd_0, 0x7c, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "rgmii")),
	[RTD1295_SB2_RGMII1_RXD_1] = RTK_PIN_MUX(rgmii1_rxd_1, 0x7c, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "rgmii")),
	[RTD1295_SB2_RGMII1_RXD_2] = RTK_PIN_MUX(rgmii1_rxd_2, 0x7c, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "rgmii")),
	[RTD1295_SB2_RGMII1_RXD_3] = RTK_PIN_MUX(rgmii1_rxd_3, 0x7c, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "rgmii")),
	[RTD1295_SB2_HI_LOC] = RTK_PIN_MUX(hi_loc, 0xc, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "hi_loc_misc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "hi_loc_nf")),
	[RTD1295_SB2_EJTAG_SCPU_LOC] = RTK_PIN_MUX(ejtag_scpu_loc, 0xc, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "scpu_ejtag_loc_gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "scpu_ejtag_loc_cr")),
	[RTD1295_SB2_SF_EN] = RTK_PIN_MUX(sf_en, 0x14, GENMASK(0, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "spi")),
	[RTD1295_SB2_ARM_TRACE_DBG_EN] = RTK_PIN_MUX(arm_trace_dbg_en, 0x14, GENMASK(1, 1),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 1), "arm_trace_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 1), "arm_trace_enable")),
	[RTD1295_SB2_DEBUG_P2S_ENABLE] = RTK_PIN_MUX(debug_p2s_enable, 0x14, GENMASK(6, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "p2s_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "p2s_enable")),
	[RTD1295_SB2_TP0_LOC] = RTK_PIN_MUX(tp0_loc, 0x14, GENMASK(7, 7),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 7), "tp0_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 7), "tp0_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 7), "tp0_loc_rgmii0_tx")),
	[RTD1295_SB2_TP1_LOC] = RTK_PIN_MUX(tp1_loc, 0x14, GENMASK(8, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "tp1_loc_tp1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "tp1_loc_tp0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "tp1_loc_rgmii0_rx")),
};

static const struct rtd_pin_config_desc rtd1295_sb2_configs[RTD1295_SB2_NUM_PINS] = {
	[RTD1295_SB2_GPIO_0] = RTK_PIN_CONFIG(gpio_0, 0x34, 0, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_GPIO_1] = RTK_PIN_CONFIG(gpio_1, 0x34, 4, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_GPIO_2] = RTK_PIN_CONFIG(gpio_2, 0x34, 8, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_GPIO_3] = RTK_PIN_CONFIG(gpio_3, 0x34, 12, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_GPIO_4] = RTK_PIN_CONFIG(gpio_4, 0x34, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_GPIO_5] = RTK_PIN_CONFIG(gpio_5, 0x34, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_GPIO_6] = RTK_PIN_CONFIG(gpio_6, 0x34, 24, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_GPIO_7] = RTK_PIN_CONFIG(gpio_7, 0x34, 28, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_GPIO_8] = RTK_PIN_CONFIG(gpio_8, 0x38, 0, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_GPIO_9] = RTK_PIN_CONFIG(gpio_9, 0x38, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP1_SYNC] = RTK_PIN_CONFIG(tp1_sync, 0x2c, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_I2C_SCL_4] = RTK_PIN_CONFIG(i2c_scl_4, 0x30, 12, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_I2C_SDA_4] = RTK_PIN_CONFIG(i2c_sda_4, 0x30, 8, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_I2C_SCL_5] = RTK_PIN_CONFIG(i2c_scl_5, 0x30, 20, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_I2C_SDA_5] = RTK_PIN_CONFIG(i2c_sda_5, 0x30, 16, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_SB2_USB_ID] = RTK_PIN_CONFIG(usb_id, 0x38, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_SENSOR_CKO_0] = RTK_PIN_CONFIG(sensor_cko_0, 0x38, 24, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_SENSOR_CKO_1] = RTK_PIN_CONFIG(sensor_cko_1, 0x38, 28, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_SENSOR_RST] = RTK_PIN_CONFIG(sensor_rst, 0x38, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_SENSOR_STB_0] = RTK_PIN_CONFIG(sensor_stb_0, 0x38, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_SENSOR_STB_1] = RTK_PIN_CONFIG(sensor_stb_1, 0x38, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP0_DATA] = RTK_PIN_CONFIG(tp0_data, 0x28, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP0_CLK] = RTK_PIN_CONFIG(tp0_clk, 0x28, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP0_VALID] = RTK_PIN_CONFIG(tp0_valid, 0x28, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP0_SYNC] = RTK_PIN_CONFIG(tp0_sync, 0x28, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP1_DATA] = RTK_PIN_CONFIG(tp1_data, 0x2c, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP1_CLK] = RTK_PIN_CONFIG(tp1_clk, 0x2c, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_TP1_VALID] = RTK_PIN_CONFIG(tp1_valid, 0x2c, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TXC] = RTK_PIN_CONFIG(rgmii0_txc, 0x64, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TX_CTL] = RTK_PIN_CONFIG(rgmii0_tx_ctl, 0x64, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TXD_0] = RTK_PIN_CONFIG(rgmii0_txd_0, 0x64, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TXD_1] = RTK_PIN_CONFIG(rgmii0_txd_1, 0x64, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TXD_2] = RTK_PIN_CONFIG(rgmii0_txd_2, 0x64, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_TXD_3] = RTK_PIN_CONFIG(rgmii0_txd_3, 0x64, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RXC] = RTK_PIN_CONFIG(rgmii0_rxc, 0x64, 24, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RX_CTL] = RTK_PIN_CONFIG(rgmii0_rx_ctl, 0x64, 28, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RXD_0] = RTK_PIN_CONFIG(rgmii0_rxd_0, 0x68, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RXD_1] = RTK_PIN_CONFIG(rgmii0_rxd_1, 0x68, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RXD_2] = RTK_PIN_CONFIG(rgmii0_rxd_2, 0x68, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_RXD_3] = RTK_PIN_CONFIG(rgmii0_rxd_3, 0x68, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_MDIO] = RTK_PIN_CONFIG(rgmii0_mdio, 0x68, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII0_MDC] = RTK_PIN_CONFIG(rgmii0_mdc, 0x68, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TXC] = RTK_PIN_CONFIG(rgmii1_txc, 0x74, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TX_CTL] = RTK_PIN_CONFIG(rgmii1_tx_ctl, 0x74, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TXD_0] = RTK_PIN_CONFIG(rgmii1_txd_0, 0x74, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TXD_1] = RTK_PIN_CONFIG(rgmii1_txd_1, 0x74, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TXD_2] = RTK_PIN_CONFIG(rgmii1_txd_2, 0x74, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_TXD_3] = RTK_PIN_CONFIG(rgmii1_txd_3, 0x74, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RXC] = RTK_PIN_CONFIG(rgmii1_rxc, 0x74, 24, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RX_CTL] = RTK_PIN_CONFIG(rgmii1_rx_ctl, 0x74, 28, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RXD_0] = RTK_PIN_CONFIG(rgmii1_rxd_0, 0x78, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RXD_1] = RTK_PIN_CONFIG(rgmii1_rxd_1, 0x78, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RXD_2] = RTK_PIN_CONFIG(rgmii1_rxd_2, 0x78, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_SB2_RGMII1_RXD_3] = RTK_PIN_CONFIG(rgmii1_rxd_3, 0x78, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
};

static const struct rtd_pinctrl_desc rtd1295_sb2_pinctrl_desc = {
	.pins = rtd1295_sb2_pins,
	.num_pins = ARRAY_SIZE(rtd1295_sb2_pins),
	.groups = rtd1295_sb2_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1295_sb2_pin_groups),
	.functions = rtd1295_sb2_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1295_sb2_pin_functions),
	.muxes = rtd1295_sb2_muxes,
	.num_muxes = ARRAY_SIZE(rtd1295_sb2_muxes),
	.configs = rtd1295_sb2_configs,
	.num_configs = ARRAY_SIZE(rtd1295_sb2_configs),
};

enum rtd1295_disp_pins {
	RTD1295_DISP_SPDIF = 0,
	RTD1295_DISP_DMIC_CLK,
	RTD1295_DISP_DMIC_DATA,
	RTD1295_DISP_AO_LRCK,
	RTD1295_DISP_AO_BCK,
	RTD1295_DISP_AOCK,
	RTD1295_DISP_AO_SD_0,
	RTD1295_DISP_AO_SD_1,
	RTD1295_DISP_AO_SD_2,
	RTD1295_DISP_AO_SD_3,
	RTD1295_DISP_AI_LOC,
	RTD1295_DISP_NUM_PINS
};

static const struct pinctrl_pin_desc rtd1295_disp_pins[] = {
	PINCTRL_PIN(RTD1295_DISP_SPDIF, "spdif"),
	PINCTRL_PIN(RTD1295_DISP_DMIC_CLK, "dmic_clk"),
	PINCTRL_PIN(RTD1295_DISP_DMIC_DATA, "dmic_data"),
	PINCTRL_PIN(RTD1295_DISP_AO_LRCK, "ao_lrck"),
	PINCTRL_PIN(RTD1295_DISP_AO_BCK, "ao_bck"),
	PINCTRL_PIN(RTD1295_DISP_AOCK, "aock"),
	PINCTRL_PIN(RTD1295_DISP_AO_SD_0, "ao_sd_0"),
	PINCTRL_PIN(RTD1295_DISP_AO_SD_1, "ao_sd_1"),
	PINCTRL_PIN(RTD1295_DISP_AO_SD_2, "ao_sd_2"),
	PINCTRL_PIN(RTD1295_DISP_AO_SD_3, "ao_sd_3"),
	PINCTRL_PIN(RTD1295_DISP_AI_LOC, "ai_loc"),
};

DECLARE_RTD1295_PIN(disp, RTD1295_DISP_SPDIF, spdif);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_DMIC_CLK, dmic_clk);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_DMIC_DATA, dmic_data);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_LRCK, ao_lrck);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_BCK, ao_bck);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AOCK, aock);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_SD_0, ao_sd_0);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_SD_1, ao_sd_1);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_SD_2, ao_sd_2);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AO_SD_3, ao_sd_3);
DECLARE_RTD1295_PIN(disp, RTD1295_DISP_AI_LOC, ai_loc);

static const struct rtd_pin_group_desc rtd1295_disp_pin_groups[] = {
	RTD1295_GROUP(disp, spdif),
	RTD1295_GROUP(disp, dmic_clk),
	RTD1295_GROUP(disp, dmic_data),
	RTD1295_GROUP(disp, ao_lrck),
	RTD1295_GROUP(disp, ao_bck),
	RTD1295_GROUP(disp, aock),
	RTD1295_GROUP(disp, ao_sd_0),
	RTD1295_GROUP(disp, ao_sd_1),
	RTD1295_GROUP(disp, ao_sd_2),
	RTD1295_GROUP(disp, ao_sd_3),
	RTD1295_GROUP(disp, ai_loc),
};

static const char * const rtd1295_disp_gpio_groups[] = {
	"spdif", "dmic_clk", "dmic_data", "ao_lrck", "ao_bck", "aock",
	"ao_sd_0", "ao_sd_1", "ao_sd_2", "ao_sd_3"
};
static const char * const rtd1295_disp_ai_groups[] = {
	"dmic_clk", "dmic_data", "ao_sd_2", "ao_sd_3"
};
static const char * const rtd1295_disp_ai_loc_disp_groups[] = { "ai_loc" };
static const char * const rtd1295_disp_ai_loc_main_groups[] = { "ai_loc" };
static const char * const rtd1295_disp_ao_groups[] = {
	"ao_lrck", "ao_bck", "aock", "ao_sd_0", "ao_sd_1", "ao_sd_2", "ao_sd_3"
};
static const char * const rtd1295_disp_dmic_groups[] = { "dmic_clk", "dmic_data" };
static const char * const rtd1295_disp_spdif_out_groups[] = { "spdif" };

static const struct rtd_pin_func_desc rtd1295_disp_pin_functions[] = {
	RTD1295_FUNC(disp, gpio),
	RTD1295_FUNC(disp, ai),
	RTD1295_FUNC(disp, ai_loc_disp),
	RTD1295_FUNC(disp, ai_loc_main),
	RTD1295_FUNC(disp, ao),
	RTD1295_FUNC(disp, dmic),
	RTD1295_FUNC(disp, spdif_out),
};

static const struct rtd_pin_desc rtd1295_disp_muxes[RTD1295_DISP_NUM_PINS] = {
	[RTD1295_DISP_SPDIF] = RTK_PIN_MUX(spdif, 0x8, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "spdif_out")),
	[RTD1295_DISP_DMIC_CLK] = RTK_PIN_MUX(dmic_clk, 0x8, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 2), "ai")),
	[RTD1295_DISP_DMIC_DATA] = RTK_PIN_MUX(dmic_data, 0x8, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "ai")),
	[RTD1295_DISP_AO_LRCK] = RTK_PIN_MUX(ao_lrck, 0x8, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "ao")),
	[RTD1295_DISP_AO_BCK] = RTK_PIN_MUX(ao_bck, 0x8, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "ao")),
	[RTD1295_DISP_AOCK] = RTK_PIN_MUX(aock, 0x8, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "ao")),
	[RTD1295_DISP_AO_SD_0] = RTK_PIN_MUX(ao_sd_0, 0x8, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "ao")),
	[RTD1295_DISP_AO_SD_1] = RTK_PIN_MUX(ao_sd_1, 0x8, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "ao")),
	[RTD1295_DISP_AO_SD_2] = RTK_PIN_MUX(ao_sd_2, 0x8, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "ao"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "ai")),
	[RTD1295_DISP_AO_SD_3] = RTK_PIN_MUX(ao_sd_3, 0x8, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "ao"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "ai")),
	[RTD1295_DISP_AI_LOC] = RTK_PIN_MUX(ai_loc, 0x8, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "ai_loc_main"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "ai_loc_disp")),
};

static const struct rtd_pin_config_desc rtd1295_disp_configs[RTD1295_DISP_NUM_PINS] = {
	[RTD1295_DISP_SPDIF] = RTK_PIN_CONFIG(spdif, 0x0, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_DMIC_CLK] = RTK_PIN_CONFIG(dmic_clk, 0x0, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_DMIC_DATA] = RTK_PIN_CONFIG(dmic_data, 0x0, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_LRCK] = RTK_PIN_CONFIG(ao_lrck, 0x4, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_BCK] = RTK_PIN_CONFIG(ao_bck, 0x4, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AOCK] = RTK_PIN_CONFIG(aock, 0x4, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_SD_0] = RTK_PIN_CONFIG(ao_sd_0, 0x4, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_SD_1] = RTK_PIN_CONFIG(ao_sd_1, 0x4, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_SD_2] = RTK_PIN_CONFIG(ao_sd_2, 0x4, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_DISP_AO_SD_3] = RTK_PIN_CONFIG(ao_sd_3, 0x4, 24, 1, 0, 2, 3, NA, PADDRI_2_4),
};

static const struct rtd_pinctrl_desc rtd1295_disp_pinctrl_desc = {
	.pins = rtd1295_disp_pins,
	.num_pins = ARRAY_SIZE(rtd1295_disp_pins),
	.groups = rtd1295_disp_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1295_disp_pin_groups),
	.functions = rtd1295_disp_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1295_disp_pin_functions),
	.muxes = rtd1295_disp_muxes,
	.num_muxes = ARRAY_SIZE(rtd1295_disp_muxes),
	.configs = rtd1295_disp_configs,
	.num_configs = ARRAY_SIZE(rtd1295_disp_configs),
};

enum rtd1295_cr_pins {
	RTD1295_CR_NF_CLE = 0,
	RTD1295_CR_NF_ALE,
	RTD1295_CR_NF_RD_N,
	RTD1295_CR_NF_WR_N,
	RTD1295_CR_NF_RDY,
	RTD1295_CR_NF_DD_7,
	RTD1295_CR_NF_DD_6,
	RTD1295_CR_NF_DD_5,
	RTD1295_CR_NF_DD_4,
	RTD1295_CR_NF_DD_3,
	RTD1295_CR_NF_DD_2,
	RTD1295_CR_NF_DD_1,
	RTD1295_CR_NF_DD_0,
	RTD1295_CR_NF_DQS,
	RTD1295_CR_NF_CE_N_0,
	RTD1295_CR_NF_CE_N_1,
	RTD1295_CR_EMMC_DD_SB,
	RTD1295_CR_MMC_CMD,
	RTD1295_CR_MMC_CLK,
	RTD1295_CR_MMC_WP,
	RTD1295_CR_MMC_CD,
	RTD1295_CR_MMC_DATA_0,
	RTD1295_CR_MMC_DATA_1,
	RTD1295_CR_MMC_DATA_2,
	RTD1295_CR_MMC_DATA_3,
	RTD1295_CR_SDIO_CMD,
	RTD1295_CR_SDIO_CLK,
	RTD1295_CR_SDIO_DATA_0,
	RTD1295_CR_SDIO_DATA_1,
	RTD1295_CR_SDIO_DATA_2,
	RTD1295_CR_SDIO_DATA_3,
	RTD1295_CR_PCIE_CLKREQ_0,
	RTD1295_CR_PCIE_CLKREQ_1,
	RTD1295_CR_PROB_0,
	RTD1295_CR_PROB_1,
	RTD1295_CR_PROB_2,
	RTD1295_CR_PROB_3,
	RTD1295_CR_SDIO_LOC,
	RTD1295_CR_NUM_PINS
};

static const struct pinctrl_pin_desc rtd1295_cr_pins[] = {
	PINCTRL_PIN(RTD1295_CR_NF_CLE, "nf_cle"),
	PINCTRL_PIN(RTD1295_CR_NF_ALE, "nf_ale"),
	PINCTRL_PIN(RTD1295_CR_NF_RD_N, "nf_rd_n"),
	PINCTRL_PIN(RTD1295_CR_NF_WR_N, "nf_wr_n"),
	PINCTRL_PIN(RTD1295_CR_NF_RDY, "nf_rdy"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_7, "nf_dd_7"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_6, "nf_dd_6"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_5, "nf_dd_5"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_4, "nf_dd_4"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_3, "nf_dd_3"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_2, "nf_dd_2"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_1, "nf_dd_1"),
	PINCTRL_PIN(RTD1295_CR_NF_DD_0, "nf_dd_0"),
	PINCTRL_PIN(RTD1295_CR_NF_DQS, "nf_dqs"),
	PINCTRL_PIN(RTD1295_CR_NF_CE_N_0, "nf_ce_n_0"),
	PINCTRL_PIN(RTD1295_CR_NF_CE_N_1, "nf_ce_n_1"),
	PINCTRL_PIN(RTD1295_CR_EMMC_DD_SB, "emmc_dd_sb"),
	PINCTRL_PIN(RTD1295_CR_MMC_CMD, "mmc_cmd"),
	PINCTRL_PIN(RTD1295_CR_MMC_CLK, "mmc_clk"),
	PINCTRL_PIN(RTD1295_CR_MMC_WP, "mmc_wp"),
	PINCTRL_PIN(RTD1295_CR_MMC_CD, "mmc_cd"),
	PINCTRL_PIN(RTD1295_CR_MMC_DATA_0, "mmc_data_0"),
	PINCTRL_PIN(RTD1295_CR_MMC_DATA_1, "mmc_data_1"),
	PINCTRL_PIN(RTD1295_CR_MMC_DATA_2, "mmc_data_2"),
	PINCTRL_PIN(RTD1295_CR_MMC_DATA_3, "mmc_data_3"),
	PINCTRL_PIN(RTD1295_CR_SDIO_CMD, "sdio_cmd"),
	PINCTRL_PIN(RTD1295_CR_SDIO_CLK, "sdio_clk"),
	PINCTRL_PIN(RTD1295_CR_SDIO_DATA_0, "sdio_data_0"),
	PINCTRL_PIN(RTD1295_CR_SDIO_DATA_1, "sdio_data_1"),
	PINCTRL_PIN(RTD1295_CR_SDIO_DATA_2, "sdio_data_2"),
	PINCTRL_PIN(RTD1295_CR_SDIO_DATA_3, "sdio_data_3"),
	PINCTRL_PIN(RTD1295_CR_PCIE_CLKREQ_0, "pcie_clkreq_0"),
	PINCTRL_PIN(RTD1295_CR_PCIE_CLKREQ_1, "pcie_clkreq_1"),
	PINCTRL_PIN(RTD1295_CR_PROB_0, "prob_0"),
	PINCTRL_PIN(RTD1295_CR_PROB_1, "prob_1"),
	PINCTRL_PIN(RTD1295_CR_PROB_2, "prob_2"),
	PINCTRL_PIN(RTD1295_CR_PROB_3, "prob_3"),
	PINCTRL_PIN(RTD1295_CR_SDIO_LOC, "sdio_loc"),
};

DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_CLE, nf_cle);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_ALE, nf_ale);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_RD_N, nf_rd_n);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_WR_N, nf_wr_n);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_RDY, nf_rdy);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_7, nf_dd_7);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_6, nf_dd_6);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_5, nf_dd_5);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_4, nf_dd_4);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_3, nf_dd_3);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_2, nf_dd_2);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_1, nf_dd_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DD_0, nf_dd_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_DQS, nf_dqs);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_CE_N_0, nf_ce_n_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_NF_CE_N_1, nf_ce_n_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_EMMC_DD_SB, emmc_dd_sb);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_CMD, mmc_cmd);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_CLK, mmc_clk);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_WP, mmc_wp);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_CD, mmc_cd);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_DATA_0, mmc_data_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_DATA_1, mmc_data_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_DATA_2, mmc_data_2);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_MMC_DATA_3, mmc_data_3);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_CMD, sdio_cmd);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_CLK, sdio_clk);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_DATA_0, sdio_data_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_DATA_1, sdio_data_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_DATA_2, sdio_data_2);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_DATA_3, sdio_data_3);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PCIE_CLKREQ_0, pcie_clkreq_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PCIE_CLKREQ_1, pcie_clkreq_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PROB_0, prob_0);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PROB_1, prob_1);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PROB_2, prob_2);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_PROB_3, prob_3);
DECLARE_RTD1295_PIN(cr, RTD1295_CR_SDIO_LOC, sdio_loc);

static const struct rtd_pin_group_desc rtd1295_cr_pin_groups[] = {
	RTD1295_GROUP(cr, nf_cle),
	RTD1295_GROUP(cr, nf_ale),
	RTD1295_GROUP(cr, nf_rd_n),
	RTD1295_GROUP(cr, nf_wr_n),
	RTD1295_GROUP(cr, nf_rdy),
	RTD1295_GROUP(cr, nf_dd_7),
	RTD1295_GROUP(cr, nf_dd_6),
	RTD1295_GROUP(cr, nf_dd_5),
	RTD1295_GROUP(cr, nf_dd_4),
	RTD1295_GROUP(cr, nf_dd_3),
	RTD1295_GROUP(cr, nf_dd_2),
	RTD1295_GROUP(cr, nf_dd_1),
	RTD1295_GROUP(cr, nf_dd_0),
	RTD1295_GROUP(cr, nf_dqs),
	RTD1295_GROUP(cr, nf_ce_n_0),
	RTD1295_GROUP(cr, nf_ce_n_1),
	RTD1295_GROUP(cr, emmc_dd_sb),
	RTD1295_GROUP(cr, mmc_cmd),
	RTD1295_GROUP(cr, mmc_clk),
	RTD1295_GROUP(cr, mmc_wp),
	RTD1295_GROUP(cr, mmc_cd),
	RTD1295_GROUP(cr, mmc_data_0),
	RTD1295_GROUP(cr, mmc_data_1),
	RTD1295_GROUP(cr, mmc_data_2),
	RTD1295_GROUP(cr, mmc_data_3),
	RTD1295_GROUP(cr, sdio_cmd),
	RTD1295_GROUP(cr, sdio_clk),
	RTD1295_GROUP(cr, sdio_data_0),
	RTD1295_GROUP(cr, sdio_data_1),
	RTD1295_GROUP(cr, sdio_data_2),
	RTD1295_GROUP(cr, sdio_data_3),
	RTD1295_GROUP(cr, pcie_clkreq_0),
	RTD1295_GROUP(cr, pcie_clkreq_1),
	RTD1295_GROUP(cr, prob_0),
	RTD1295_GROUP(cr, prob_1),
	RTD1295_GROUP(cr, prob_2),
	RTD1295_GROUP(cr, prob_3),
	RTD1295_GROUP(cr, sdio_loc),
};

static const char * const rtd1295_cr_gpio_groups[] = {
	"nf_cle", "nf_ale", "nf_rd_n", "nf_wr_n", "nf_rdy", "nf_dd_7",
	"nf_dd_6", "nf_dd_5", "nf_dd_4", "nf_dd_3", "nf_dd_2", "nf_dd_1",
	"nf_dd_0", "nf_dqs", "nf_ce_n_0", "nf_ce_n_1", "emmc_dd_sb", "mmc_cmd",
	"mmc_clk", "mmc_wp", "mmc_cd", "mmc_data_0", "mmc_data_1", "mmc_data_2",
	"mmc_data_3", "sdio_cmd", "sdio_clk", "sdio_data_0", "sdio_data_1",
	"sdio_data_2", "sdio_data_3", "pcie_clkreq_0", "pcie_clkreq_1",
	"prob_0", "prob_1", "prob_2", "prob_3"
};
static const char * const rtd1295_cr_avcpu_ej_groups[] = {
	"nf_rd_n", "nf_rdy", "nf_dd_7", "nf_dd_6", "nf_dd_5"
};
static const char * const rtd1295_cr_emmc_groups[] = {
	"nf_cle", "nf_rd_n", "nf_rdy", "nf_dd_7", "nf_dd_6", "nf_dd_5",
	"nf_dd_4", "nf_dd_3", "nf_dd_2", "nf_dd_1", "nf_dd_0", "emmc_dd_sb"
};
static const char * const rtd1295_cr_hif_groups[] = { "nf_cle", "nf_ale", "nf_wr_n", "nf_dd_4" };
static const char * const rtd1295_cr_nand_groups[] = {
	"nf_cle", "nf_ale", "nf_rd_n", "nf_wr_n", "nf_rdy", "nf_dd_7",
	"nf_dd_6", "nf_dd_5", "nf_dd_4", "nf_dd_3", "nf_dd_2", "nf_dd_1",
	"nf_dd_0", "nf_dqs", "nf_ce_n_0", "nf_ce_n_1"
};
static const char * const rtd1295_cr_p2s_groups[] = { "prob_0", "prob_1" };
static const char * const rtd1295_cr_pcie_groups[] = { "pcie_clkreq_0", "pcie_clkreq_1" };
static const char * const rtd1295_cr_pll_test_groups[] = { "prob_0", "prob_1", "prob_2", "prob_3" };
static const char * const rtd1295_cr_scpu_ejtag_loc_cr_groups[] = {
	"mmc_cmd", "mmc_clk", "mmc_wp", "mmc_data_0", "mmc_data_3"
};
static const char * const rtd1295_cr_sd_card_groups[] = {
	"mmc_cmd", "mmc_clk", "mmc_wp", "mmc_cd", "mmc_data_0", "mmc_data_1",
	"mmc_data_2", "mmc_data_3"
};
static const char * const rtd1295_cr_sdio_groups[] = {
	"mmc_cmd", "mmc_clk", "mmc_data_0", "mmc_data_1", "mmc_data_2",
	"mmc_data_3", "sdio_cmd", "sdio_clk", "sdio_data_0", "sdio_data_1",
	"sdio_data_2", "sdio_data_3"
};
static const char * const rtd1295_cr_sdio_loc_mmc_groups[] = { "sdio_loc" };
static const char * const rtd1295_cr_sdio_loc_sdio_groups[] = { "sdio_loc" };

static const struct rtd_pin_func_desc rtd1295_cr_pin_functions[] = {
	RTD1295_FUNC(cr, gpio),
	RTD1295_FUNC(cr, avcpu_ej),
	RTD1295_FUNC(cr, emmc),
	RTD1295_FUNC(cr, hif),
	RTD1295_FUNC(cr, nand),
	RTD1295_FUNC(cr, p2s),
	RTD1295_FUNC(cr, pcie),
	RTD1295_FUNC(cr, pll_test),
	RTD1295_FUNC(cr, scpu_ejtag_loc_cr),
	RTD1295_FUNC(cr, sd_card),
	RTD1295_FUNC(cr, sdio),
	RTD1295_FUNC(cr, sdio_loc_mmc),
	RTD1295_FUNC(cr, sdio_loc_sdio),
};

static const struct rtd_pin_desc rtd1295_cr_muxes[RTD1295_CR_NUM_PINS] = {
	[RTD1295_CR_NF_CLE] = RTK_PIN_MUX(nf_cle, 0x0, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 10), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 10), "hif")),
	[RTD1295_CR_NF_ALE] = RTK_PIN_MUX(nf_ale, 0x0, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "hif")),
	[RTD1295_CR_NF_RD_N] = RTK_PIN_MUX(nf_rd_n, 0x0, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "avcpu_ej")),
	[RTD1295_CR_NF_WR_N] = RTK_PIN_MUX(nf_wr_n, 0x0, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 6), "hif")),
	[RTD1295_CR_NF_RDY] = RTK_PIN_MUX(nf_rdy, 0x0, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 2), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 2), "avcpu_ej")),
	[RTD1295_CR_NF_DD_7] = RTK_PIN_MUX(nf_dd_7, 0x0, GENMASK(31, 30),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 30), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 30), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 30), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 30), "avcpu_ej")),
	[RTD1295_CR_NF_DD_6] = RTK_PIN_MUX(nf_dd_6, 0x0, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 28), "avcpu_ej")),
	[RTD1295_CR_NF_DD_5] = RTK_PIN_MUX(nf_dd_5, 0x0, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 26), "avcpu_ej")),
	[RTD1295_CR_NF_DD_4] = RTK_PIN_MUX(nf_dd_4, 0x0, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "hif")),
	[RTD1295_CR_NF_DD_3] = RTK_PIN_MUX(nf_dd_3, 0x0, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 22), "emmc")),
	[RTD1295_CR_NF_DD_2] = RTK_PIN_MUX(nf_dd_2, 0x0, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "emmc")),
	[RTD1295_CR_NF_DD_1] = RTK_PIN_MUX(nf_dd_1, 0x0, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "emmc")),
	[RTD1295_CR_NF_DD_0] = RTK_PIN_MUX(nf_dd_0, 0x0, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "nand"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "emmc")),
	[RTD1295_CR_NF_DQS] = RTK_PIN_MUX(nf_dqs, 0x0, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "nand")),
	[RTD1295_CR_NF_CE_N_0] = RTK_PIN_MUX(nf_ce_n_0, 0x0, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "nand")),
	[RTD1295_CR_NF_CE_N_1] = RTK_PIN_MUX(nf_ce_n_1, 0x0, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "nand")),
	[RTD1295_CR_EMMC_DD_SB] = RTK_PIN_MUX(emmc_dd_sb, 0x4, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "emmc")),
	[RTD1295_CR_MMC_CMD] = RTK_PIN_MUX(mmc_cmd, 0x4, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "sdio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "scpu_ejtag_loc_cr")),
	[RTD1295_CR_MMC_CLK] = RTK_PIN_MUX(mmc_clk, 0x4, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "sdio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 18), "scpu_ejtag_loc_cr")),
	[RTD1295_CR_MMC_WP] = RTK_PIN_MUX(mmc_wp, 0x4, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "scpu_ejtag_loc_cr")),
	[RTD1295_CR_MMC_CD] = RTK_PIN_MUX(mmc_cd, 0x4, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "sd_card")),
	[RTD1295_CR_MMC_DATA_0] = RTK_PIN_MUX(mmc_data_0, 0x4, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "sdio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "scpu_ejtag_loc_cr")),
	[RTD1295_CR_MMC_DATA_1] = RTK_PIN_MUX(mmc_data_1, 0x4, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "sdio")),
	[RTD1295_CR_MMC_DATA_2] = RTK_PIN_MUX(mmc_data_2, 0x4, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "sdio")),
	[RTD1295_CR_MMC_DATA_3] = RTK_PIN_MUX(mmc_data_3, 0x4, GENMASK(31, 30),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 30), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 30), "sd_card"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 30), "sdio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 30), "scpu_ejtag_loc_cr")),
	[RTD1295_CR_SDIO_CMD] = RTK_PIN_MUX(sdio_cmd, 0x4, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "sdio")),
	[RTD1295_CR_SDIO_CLK] = RTK_PIN_MUX(sdio_clk, 0x4, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "sdio")),
	[RTD1295_CR_SDIO_DATA_0] = RTK_PIN_MUX(sdio_data_0, 0x4, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "sdio")),
	[RTD1295_CR_SDIO_DATA_1] = RTK_PIN_MUX(sdio_data_1, 0x4, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "sdio")),
	[RTD1295_CR_SDIO_DATA_2] = RTK_PIN_MUX(sdio_data_2, 0x4, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "sdio")),
	[RTD1295_CR_SDIO_DATA_3] = RTK_PIN_MUX(sdio_data_3, 0x4, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "sdio")),
	[RTD1295_CR_PCIE_CLKREQ_0] = RTK_PIN_MUX(pcie_clkreq_0, 0x1c, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "pcie")),
	[RTD1295_CR_PCIE_CLKREQ_1] = RTK_PIN_MUX(pcie_clkreq_1, 0x1c, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "pcie")),
	[RTD1295_CR_PROB_0] = RTK_PIN_MUX(prob_0, 0x1c, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "pll_test"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 6), "p2s")),
	[RTD1295_CR_PROB_1] = RTK_PIN_MUX(prob_1, 0x1c, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "pll_test"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "p2s")),
	[RTD1295_CR_PROB_2] = RTK_PIN_MUX(prob_2, 0x1c, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "pll_test")),
	[RTD1295_CR_PROB_3] = RTK_PIN_MUX(prob_3, 0x1c, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "pll_test")),
	[RTD1295_CR_SDIO_LOC] = RTK_PIN_MUX(sdio_loc, 0x1c, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "sdio_loc_sdio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "sdio_loc_mmc")),
};

static const struct rtd_pin_config_desc rtd1295_cr_configs[RTD1295_CR_NUM_PINS] = {
	[RTD1295_CR_NF_CLE] = RTK_PIN_CONFIG(nf_cle, 0x8, 28, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_ALE] = RTK_PIN_CONFIG(nf_ale, 0x8, 4, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_RD_N] = RTK_PIN_CONFIG(nf_rd_n, 0x8, 20, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_WR_N] = RTK_PIN_CONFIG(nf_wr_n, 0x8, 24, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_RDY] = RTK_PIN_CONFIG(nf_rdy, 0x8, 16, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_7] = RTK_PIN_CONFIG(nf_dd_7, 0xc, 28, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_6] = RTK_PIN_CONFIG(nf_dd_6, 0xc, 24, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_5] = RTK_PIN_CONFIG(nf_dd_5, 0xc, 20, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_4] = RTK_PIN_CONFIG(nf_dd_4, 0xc, 16, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_3] = RTK_PIN_CONFIG(nf_dd_3, 0xc, 12, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_2] = RTK_PIN_CONFIG(nf_dd_2, 0xc, 8, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_1] = RTK_PIN_CONFIG(nf_dd_1, 0xc, 4, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DD_0] = RTK_PIN_CONFIG(nf_dd_0, 0xc, 0, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_DQS] = RTK_PIN_CONFIG(nf_dqs, 0x8, 0, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_CE_N_0] = RTK_PIN_CONFIG(nf_ce_n_0, 0x8, 8, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_NF_CE_N_1] = RTK_PIN_CONFIG(nf_ce_n_1, 0x8, 12, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_EMMC_DD_SB] = RTK_PIN_CONFIG(emmc_dd_sb, 0x18, 0, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_CMD] = RTK_PIN_CONFIG(mmc_cmd, 0x10, 0, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_CLK] = RTK_PIN_CONFIG(mmc_clk, 0x10, 4, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_WP] = RTK_PIN_CONFIG(mmc_wp, 0x10, 8, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_MMC_CD] = RTK_PIN_CONFIG(mmc_cd, 0x10, 12, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_MMC_DATA_0] = RTK_PIN_CONFIG(mmc_data_0, 0x10, 16, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_DATA_1] = RTK_PIN_CONFIG(mmc_data_1, 0x10, 20, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_DATA_2] = RTK_PIN_CONFIG(mmc_data_2, 0x10, 24, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_MMC_DATA_3] = RTK_PIN_CONFIG(mmc_data_3, 0x10, 28, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_CMD] = RTK_PIN_CONFIG(sdio_cmd, 0x14, 0, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_CLK] = RTK_PIN_CONFIG(sdio_clk, 0x14, 4, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_DATA_0] = RTK_PIN_CONFIG(sdio_data_0, 0x14, 16, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_DATA_1] = RTK_PIN_CONFIG(sdio_data_1, 0x14, 20, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_DATA_2] = RTK_PIN_CONFIG(sdio_data_2, 0x14, 24, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_SDIO_DATA_3] = RTK_PIN_CONFIG(sdio_data_3, 0x14, 28, 1, 0, 2, 3, NA, NA),
	[RTD1295_CR_PCIE_CLKREQ_0] = RTK_PIN_CONFIG(pcie_clkreq_0, 0x18, 4, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_PCIE_CLKREQ_1] = RTK_PIN_CONFIG(pcie_clkreq_1, 0x18, 8, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_PROB_0] = RTK_PIN_CONFIG(prob_0, 0x18, 12, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_PROB_1] = RTK_PIN_CONFIG(prob_1, 0x18, 16, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_PROB_2] = RTK_PIN_CONFIG(prob_2, 0x18, 20, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_CR_PROB_3] = RTK_PIN_CONFIG(prob_3, 0x18, 24, 1, 0, 2, 3, NA, PADDRI_4_8),
};

static const struct rtd_pinctrl_desc rtd1295_cr_pinctrl_desc = {
	.pins = rtd1295_cr_pins,
	.num_pins = ARRAY_SIZE(rtd1295_cr_pins),
	.groups = rtd1295_cr_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1295_cr_pin_groups),
	.functions = rtd1295_cr_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1295_cr_pin_functions),
	.muxes = rtd1295_cr_muxes,
	.num_muxes = ARRAY_SIZE(rtd1295_cr_muxes),
	.configs = rtd1295_cr_configs,
	.num_configs = ARRAY_SIZE(rtd1295_cr_configs),
};

enum rtd1295_iso_pins {
	RTD1295_ISO_ISO_GPIO_0 = 0,
	RTD1295_ISO_ISO_GPIO_1,
	RTD1295_ISO_ISO_GPIO_2,
	RTD1295_ISO_ISO_GPIO_3,
	RTD1295_ISO_ISO_GPIO_4,
	RTD1295_ISO_ISO_GPIO_5,
	RTD1295_ISO_HDMI_HPD,
	RTD1295_ISO_ISO_GPIO_7,
	RTD1295_ISO_IR_RX,
	RTD1295_ISO_IR_TX,
	RTD1295_ISO_UR0_RX,
	RTD1295_ISO_UR0_TX,
	RTD1295_ISO_UR1_RX,
	RTD1295_ISO_UR1_TX,
	RTD1295_ISO_UR1_CTS_N,
	RTD1295_ISO_UR1_RTS_N,
	RTD1295_ISO_I2C_SCL_0,
	RTD1295_ISO_I2C_SDA_0,
	RTD1295_ISO_I2C_SCL_1,
	RTD1295_ISO_I2C_SDA_1,
	RTD1295_ISO_I2C_SCL_6,
	RTD1295_ISO_ISO_GPIO_21,
	RTD1295_ISO_ISO_GPIO_22,
	RTD1295_ISO_ISO_GPIO_23,
	RTD1295_ISO_ISO_GPIO_24,
	RTD1295_ISO_ISO_GPIO_25,
	RTD1295_ISO_I2C_SDA_6,
	RTD1295_ISO_ETN_LED_LINK,
	RTD1295_ISO_ETN_LED_RXTX,
	RTD1295_ISO_NAT_LED_0,
	RTD1295_ISO_NAT_LED_1,
	RTD1295_ISO_NAT_LED_2,
	RTD1295_ISO_NAT_LED_3,
	RTD1295_ISO_ISO_GPIO_33,
	RTD1295_ISO_ISO_GPIO_34,
	RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC0,
	RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC0,
	RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC1,
	RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC1,
	RTD1295_ISO_EJTAG_AVCPU_LOC,
	RTD1295_ISO_UR2_LOC,
	RTD1295_ISO_I2C_TG_ENABLE,
	RTD1295_ISO_NUM_PINS
};

static const struct pinctrl_pin_desc rtd1295_iso_pins[] = {
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_0, "iso_gpio_0"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_1, "iso_gpio_1"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_2, "iso_gpio_2"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_3, "iso_gpio_3"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_4, "iso_gpio_4"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_5, "iso_gpio_5"),
	PINCTRL_PIN(RTD1295_ISO_HDMI_HPD, "hdmi_hpd"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_7, "iso_gpio_7"),
	PINCTRL_PIN(RTD1295_ISO_IR_RX, "ir_rx"),
	PINCTRL_PIN(RTD1295_ISO_IR_TX, "ir_tx"),
	PINCTRL_PIN(RTD1295_ISO_UR0_RX, "ur0_rx"),
	PINCTRL_PIN(RTD1295_ISO_UR0_TX, "ur0_tx"),
	PINCTRL_PIN(RTD1295_ISO_UR1_RX, "ur1_rx"),
	PINCTRL_PIN(RTD1295_ISO_UR1_TX, "ur1_tx"),
	PINCTRL_PIN(RTD1295_ISO_UR1_CTS_N, "ur1_cts_n"),
	PINCTRL_PIN(RTD1295_ISO_UR1_RTS_N, "ur1_rts_n"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SCL_0, "i2c_scl_0"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SDA_0, "i2c_sda_0"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SCL_1, "i2c_scl_1"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SDA_1, "i2c_sda_1"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SCL_6, "i2c_scl_6"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_21, "iso_gpio_21"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_22, "iso_gpio_22"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_23, "iso_gpio_23"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_24, "iso_gpio_24"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_25, "iso_gpio_25"),
	PINCTRL_PIN(RTD1295_ISO_I2C_SDA_6, "i2c_sda_6"),
	PINCTRL_PIN(RTD1295_ISO_ETN_LED_LINK, "etn_led_link"),
	PINCTRL_PIN(RTD1295_ISO_ETN_LED_RXTX, "etn_led_rxtx"),
	PINCTRL_PIN(RTD1295_ISO_NAT_LED_0, "nat_led_0"),
	PINCTRL_PIN(RTD1295_ISO_NAT_LED_1, "nat_led_1"),
	PINCTRL_PIN(RTD1295_ISO_NAT_LED_2, "nat_led_2"),
	PINCTRL_PIN(RTD1295_ISO_NAT_LED_3, "nat_led_3"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_33, "iso_gpio_33"),
	PINCTRL_PIN(RTD1295_ISO_ISO_GPIO_34, "iso_gpio_34"),
	PINCTRL_PIN(RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC0, "pwm_23_open_drain_en_loc0"),
	PINCTRL_PIN(RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC0, "pwm_01_open_drain_en_loc0"),
	PINCTRL_PIN(RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC1, "pwm_23_open_drain_en_loc1"),
	PINCTRL_PIN(RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC1, "pwm_01_open_drain_en_loc1"),
	PINCTRL_PIN(RTD1295_ISO_EJTAG_AVCPU_LOC, "ejtag_avcpu_loc"),
	PINCTRL_PIN(RTD1295_ISO_UR2_LOC, "ur2_loc"),
	PINCTRL_PIN(RTD1295_ISO_I2C_TG_ENABLE, "i2c_tg_enable"),
};

DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_0, iso_gpio_0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_1, iso_gpio_1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_2, iso_gpio_2);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_3, iso_gpio_3);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_4, iso_gpio_4);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_5, iso_gpio_5);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_HDMI_HPD, hdmi_hpd);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_7, iso_gpio_7);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_IR_RX, ir_rx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_IR_TX, ir_tx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR0_RX, ur0_rx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR0_TX, ur0_tx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR1_RX, ur1_rx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR1_TX, ur1_tx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR1_CTS_N, ur1_cts_n);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR1_RTS_N, ur1_rts_n);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SCL_0, i2c_scl_0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SDA_0, i2c_sda_0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SCL_1, i2c_scl_1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SDA_1, i2c_sda_1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SCL_6, i2c_scl_6);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_21, iso_gpio_21);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_22, iso_gpio_22);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_23, iso_gpio_23);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_24, iso_gpio_24);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_25, iso_gpio_25);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_SDA_6, i2c_sda_6);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ETN_LED_LINK, etn_led_link);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ETN_LED_RXTX, etn_led_rxtx);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_NAT_LED_0, nat_led_0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_NAT_LED_1, nat_led_1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_NAT_LED_2, nat_led_2);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_NAT_LED_3, nat_led_3);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_33, iso_gpio_33);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_ISO_GPIO_34, iso_gpio_34);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC0, pwm_23_open_drain_en_loc0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC0, pwm_01_open_drain_en_loc0);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC1, pwm_23_open_drain_en_loc1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC1, pwm_01_open_drain_en_loc1);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_EJTAG_AVCPU_LOC, ejtag_avcpu_loc);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_UR2_LOC, ur2_loc);
DECLARE_RTD1295_PIN(iso, RTD1295_ISO_I2C_TG_ENABLE, i2c_tg_enable);

static const struct rtd_pin_group_desc rtd1295_iso_pin_groups[] = {
	RTD1295_GROUP(iso, iso_gpio_0),
	RTD1295_GROUP(iso, iso_gpio_1),
	RTD1295_GROUP(iso, iso_gpio_2),
	RTD1295_GROUP(iso, iso_gpio_3),
	RTD1295_GROUP(iso, iso_gpio_4),
	RTD1295_GROUP(iso, iso_gpio_5),
	RTD1295_GROUP(iso, hdmi_hpd),
	RTD1295_GROUP(iso, iso_gpio_7),
	RTD1295_GROUP(iso, ir_rx),
	RTD1295_GROUP(iso, ir_tx),
	RTD1295_GROUP(iso, ur0_rx),
	RTD1295_GROUP(iso, ur0_tx),
	RTD1295_GROUP(iso, ur1_rx),
	RTD1295_GROUP(iso, ur1_tx),
	RTD1295_GROUP(iso, ur1_cts_n),
	RTD1295_GROUP(iso, ur1_rts_n),
	RTD1295_GROUP(iso, i2c_scl_0),
	RTD1295_GROUP(iso, i2c_sda_0),
	RTD1295_GROUP(iso, i2c_scl_1),
	RTD1295_GROUP(iso, i2c_sda_1),
	RTD1295_GROUP(iso, i2c_scl_6),
	RTD1295_GROUP(iso, iso_gpio_21),
	RTD1295_GROUP(iso, iso_gpio_22),
	RTD1295_GROUP(iso, iso_gpio_23),
	RTD1295_GROUP(iso, iso_gpio_24),
	RTD1295_GROUP(iso, iso_gpio_25),
	RTD1295_GROUP(iso, i2c_sda_6),
	RTD1295_GROUP(iso, etn_led_link),
	RTD1295_GROUP(iso, etn_led_rxtx),
	RTD1295_GROUP(iso, nat_led_0),
	RTD1295_GROUP(iso, nat_led_1),
	RTD1295_GROUP(iso, nat_led_2),
	RTD1295_GROUP(iso, nat_led_3),
	RTD1295_GROUP(iso, iso_gpio_33),
	RTD1295_GROUP(iso, iso_gpio_34),
	RTD1295_GROUP(iso, pwm_23_open_drain_en_loc0),
	RTD1295_GROUP(iso, pwm_01_open_drain_en_loc0),
	RTD1295_GROUP(iso, pwm_23_open_drain_en_loc1),
	RTD1295_GROUP(iso, pwm_01_open_drain_en_loc1),
	RTD1295_GROUP(iso, ejtag_avcpu_loc),
	RTD1295_GROUP(iso, ur2_loc),
	RTD1295_GROUP(iso, i2c_tg_enable),
};

static const char * const rtd1295_iso_gpio_groups[] = {
	"iso_gpio_0", "iso_gpio_1", "iso_gpio_2", "iso_gpio_3", "iso_gpio_4",
	"iso_gpio_5", "hdmi_hpd", "iso_gpio_7", "ir_rx", "ir_tx", "ur0_rx",
	"ur0_tx", "ur1_rx", "ur1_tx", "ur1_cts_n", "ur1_rts_n", "i2c_scl_0",
	"i2c_sda_0", "i2c_scl_1", "i2c_sda_1", "i2c_scl_6", "iso_gpio_21",
	"iso_gpio_22", "iso_gpio_23", "iso_gpio_24", "iso_gpio_25", "i2c_sda_6",
	"etn_led_link", "etn_led_rxtx", "nat_led_0", "nat_led_1", "nat_led_2",
	"nat_led_3", "iso_gpio_33", "iso_gpio_34"
};
static const char * const rtd1295_iso_acpu_ejtag_loc_iso_groups[] = {
	"iso_gpio_2", "iso_gpio_3", "iso_gpio_4", "iso_gpio_5", "iso_gpio_7",
	"ejtag_avcpu_loc"
};
static const char * const rtd1295_iso_acpu_ejtag_loc_nf_groups[] = { "ejtag_avcpu_loc" };
static const char * const rtd1295_iso_edp_hpd_groups[] = { "iso_gpio_7" };
static const char * const rtd1295_iso_etn_led_groups[] = { "etn_led_link", "etn_led_rxtx" };
static const char * const rtd1295_iso_i2c0_groups[] = { "i2c_scl_0", "i2c_sda_0" };
static const char * const rtd1295_iso_i2c1_groups[] = { "i2c_scl_1", "i2c_sda_1" };
static const char * const rtd1295_iso_i2c6_groups[] = { "i2c_scl_6", "i2c_sda_6" };
static const char * const rtd1295_iso_i2c_tg_disable_groups[] = { "i2c_tg_enable" };
static const char * const rtd1295_iso_i2c_tg_enable_groups[] = { "i2c_tg_enable" };
static const char * const rtd1295_iso_ir_rx_groups[] = { "ir_rx" };
static const char * const rtd1295_iso_ir_tx_groups[] = { "ir_tx" };
static const char * const rtd1295_iso_nat_led_groups[] = {
	"nat_led_0", "nat_led_1", "nat_led_2", "nat_led_3"
};
static const char * const rtd1295_iso_pwm_groups[] = {
	"iso_gpio_21", "iso_gpio_22", "iso_gpio_23", "iso_gpio_24",
	"etn_led_link", "etn_led_rxtx", "nat_led_0", "nat_led_1"
};
static const char * const rtd1295_iso_pwm_normal_groups[] = {
	"pwm_23_open_drain_en_loc0", "pwm_01_open_drain_en_loc0",
	"pwm_23_open_drain_en_loc1", "pwm_01_open_drain_en_loc1"
};
static const char * const rtd1295_iso_pwm_open_drain_groups[] = {
	"pwm_23_open_drain_en_loc0", "pwm_01_open_drain_en_loc0",
	"pwm_23_open_drain_en_loc1", "pwm_01_open_drain_en_loc1"
};
static const char * const rtd1295_iso_rtc_groups[] = { "iso_gpio_25" };
static const char * const rtd1295_iso_sc_groups[] = {
	"nat_led_0", "nat_led_1", "nat_led_2", "nat_led_3"
};
static const char * const rtd1295_iso_standby_dbg_groups[] = {
	"iso_gpio_2", "iso_gpio_3", "ir_rx"
};
static const char * const rtd1295_iso_uart0_groups[] = { "ur0_rx", "ur0_tx" };
static const char * const rtd1295_iso_uart1_groups[] = {
	"ur1_rx", "ur1_tx", "ur1_cts_n", "ur1_rts_n"
};
static const char * const rtd1295_iso_uart2_0_groups[] = {
	"iso_gpio_2", "iso_gpio_3", "iso_gpio_4", "iso_gpio_5", "ur2_loc"
};
static const char * const rtd1295_iso_uart2_1_groups[] = {
	"iso_gpio_23", "iso_gpio_24", "iso_gpio_33", "iso_gpio_34", "ur2_loc"
};

static const struct rtd_pin_func_desc rtd1295_iso_pin_functions[] = {
	RTD1295_FUNC(iso, gpio),
	RTD1295_FUNC(iso, acpu_ejtag_loc_iso),
	RTD1295_FUNC(iso, acpu_ejtag_loc_nf),
	RTD1295_FUNC(iso, edp_hpd),
	RTD1295_FUNC(iso, etn_led),
	RTD1295_FUNC(iso, i2c0),
	RTD1295_FUNC(iso, i2c1),
	RTD1295_FUNC(iso, i2c6),
	RTD1295_FUNC(iso, i2c_tg_disable),
	RTD1295_FUNC(iso, i2c_tg_enable),
	RTD1295_FUNC(iso, ir_rx),
	RTD1295_FUNC(iso, ir_tx),
	RTD1295_FUNC(iso, nat_led),
	RTD1295_FUNC(iso, pwm),
	RTD1295_FUNC(iso, pwm_normal),
	RTD1295_FUNC(iso, pwm_open_drain),
	RTD1295_FUNC(iso, rtc),
	RTD1295_FUNC(iso, sc),
	RTD1295_FUNC(iso, standby_dbg),
	RTD1295_FUNC(iso, uart0),
	RTD1295_FUNC(iso, uart1),
	RTD1295_FUNC(iso, uart2_0),
	RTD1295_FUNC(iso, uart2_1),
};

static const struct rtd_pin_desc rtd1295_iso_muxes[RTD1295_ISO_NUM_PINS] = {
	[RTD1295_ISO_ISO_GPIO_2] = RTK_PIN_MUX(iso_gpio_2, 0x14, GENMASK(8, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "standby_dbg"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 6), "acpu_ejtag_loc_iso"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 6), "uart2_0")),
	[RTD1295_ISO_ISO_GPIO_3] = RTK_PIN_MUX(iso_gpio_3, 0x14, GENMASK(11, 9),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 9), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 9), "standby_dbg"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 9), "acpu_ejtag_loc_iso"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 9), "uart2_0")),
	[RTD1295_ISO_ISO_GPIO_4] = RTK_PIN_MUX(iso_gpio_4, 0x10, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart2_0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "acpu_ejtag_loc_iso")),
	[RTD1295_ISO_ISO_GPIO_5] = RTK_PIN_MUX(iso_gpio_5, 0x10, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "uart2_0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 2), "acpu_ejtag_loc_iso")),
	[RTD1295_ISO_HDMI_HPD] = RTK_PIN_MUX(hdmi_hpd, 0x14, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio")),
	[RTD1295_ISO_ISO_GPIO_7] = RTK_PIN_MUX(iso_gpio_7, 0x10, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "edp_hpd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "acpu_ejtag_loc_iso")),
	[RTD1295_ISO_IR_RX] = RTK_PIN_MUX(ir_rx, 0x10, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "ir_rx"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 6), "standby_dbg")),
	[RTD1295_ISO_IR_TX] = RTK_PIN_MUX(ir_tx, 0x10, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "ir_tx")),
	[RTD1295_ISO_UR0_RX] = RTK_PIN_MUX(ur0_rx, 0x10, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "uart0")),
	[RTD1295_ISO_UR0_TX] = RTK_PIN_MUX(ur0_tx, 0x10, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart0")),
	[RTD1295_ISO_UR1_RX] = RTK_PIN_MUX(ur1_rx, 0x10, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "uart1")),
	[RTD1295_ISO_UR1_TX] = RTK_PIN_MUX(ur1_tx, 0x10, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "uart1")),
	[RTD1295_ISO_UR1_CTS_N] = RTK_PIN_MUX(ur1_cts_n, 0x10, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "uart1")),
	[RTD1295_ISO_UR1_RTS_N] = RTK_PIN_MUX(ur1_rts_n, 0x10, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "uart1")),
	[RTD1295_ISO_I2C_SCL_0] = RTK_PIN_MUX(i2c_scl_0, 0x10, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "i2c0")),
	[RTD1295_ISO_I2C_SDA_0] = RTK_PIN_MUX(i2c_sda_0, 0x10, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "i2c0")),
	[RTD1295_ISO_I2C_SCL_1] = RTK_PIN_MUX(i2c_scl_1, 0x14, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "i2c1")),
	[RTD1295_ISO_I2C_SDA_1] = RTK_PIN_MUX(i2c_sda_1, 0x14, GENMASK(15, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 14), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "i2c1")),
	[RTD1295_ISO_I2C_SCL_6] = RTK_PIN_MUX(i2c_scl_6, 0x14, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "i2c6")),
	[RTD1295_ISO_ISO_GPIO_21] = RTK_PIN_MUX(iso_gpio_21, 0x1c, GENMASK(1, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "pwm")),
	[RTD1295_ISO_ISO_GPIO_22] = RTK_PIN_MUX(iso_gpio_22, 0x1c, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "pwm")),
	[RTD1295_ISO_ISO_GPIO_23] = RTK_PIN_MUX(iso_gpio_23, 0x1c, GENMASK(5, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "pwm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "uart2_1")),
	[RTD1295_ISO_ISO_GPIO_24] = RTK_PIN_MUX(iso_gpio_24, 0x1c, GENMASK(7, 6),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 6), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 6), "pwm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 6), "uart2_1")),
	[RTD1295_ISO_ISO_GPIO_25] = RTK_PIN_MUX(iso_gpio_25, 0x1c, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "rtc")),
	[RTD1295_ISO_I2C_SDA_6] = RTK_PIN_MUX(i2c_sda_6, 0x14, GENMASK(3, 2),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 2), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 2), "i2c6")),
	[RTD1295_ISO_ETN_LED_LINK] = RTK_PIN_MUX(etn_led_link, 0x10, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "etn_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "pwm")),
	[RTD1295_ISO_ETN_LED_RXTX] = RTK_PIN_MUX(etn_led_rxtx, 0x10, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "etn_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "pwm")),
	[RTD1295_ISO_NAT_LED_0] = RTK_PIN_MUX(nat_led_0, 0x14, GENMASK(17, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "nat_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "pwm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "sc")),
	[RTD1295_ISO_NAT_LED_1] = RTK_PIN_MUX(nat_led_1, 0x14, GENMASK(19, 18),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 18), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 18), "nat_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 18), "pwm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 18), "sc")),
	[RTD1295_ISO_NAT_LED_2] = RTK_PIN_MUX(nat_led_2, 0x14, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "nat_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "sc")),
	[RTD1295_ISO_NAT_LED_3] = RTK_PIN_MUX(nat_led_3, 0x14, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 22), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "nat_led"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 22), "sc")),
	[RTD1295_ISO_ISO_GPIO_33] = RTK_PIN_MUX(iso_gpio_33, 0x1c, GENMASK(11, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 10), "uart2_1")),
	[RTD1295_ISO_ISO_GPIO_34] = RTK_PIN_MUX(iso_gpio_34, 0x1c, GENMASK(13, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "uart2_1")),
	[RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC0] = RTK_PIN_MUX(pwm_23_open_drain_en_loc0, 0x14, GENMASK(24, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "pwm_normal"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "pwm_open_drain")),
	[RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC0] = RTK_PIN_MUX(pwm_01_open_drain_en_loc0, 0x14, GENMASK(25, 25),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 25), "pwm_normal"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 25), "pwm_open_drain")),
	[RTD1295_ISO_PWM_23_OPEN_DRAIN_EN_LOC1] = RTK_PIN_MUX(pwm_23_open_drain_en_loc1, 0x14, GENMASK(26, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 26), "pwm_normal"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "pwm_open_drain")),
	[RTD1295_ISO_PWM_01_OPEN_DRAIN_EN_LOC1] = RTK_PIN_MUX(pwm_01_open_drain_en_loc1, 0x14, GENMASK(27, 27),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 27), "pwm_normal"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 27), "pwm_open_drain")),
	[RTD1295_ISO_EJTAG_AVCPU_LOC] = RTK_PIN_MUX(ejtag_avcpu_loc, 0x14, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "acpu_ejtag_loc_iso"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "acpu_ejtag_loc_nf")),
	[RTD1295_ISO_UR2_LOC] = RTK_PIN_MUX(ur2_loc, 0x14, GENMASK(31, 30),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 30), "uart2_0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 30), "uart2_1")),
	[RTD1295_ISO_I2C_TG_ENABLE] = RTK_PIN_MUX(i2c_tg_enable, 0x1c, GENMASK(15, 15),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 15), "i2c_tg_enable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 15), "i2c_tg_disable")),
};

static const struct rtd_pin_config_desc rtd1295_iso_configs[RTD1295_ISO_NUM_PINS] = {
	[RTD1295_ISO_ISO_GPIO_2] = RTK_PIN_CONFIG(iso_gpio_2, 0x0, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_3] = RTK_PIN_CONFIG(iso_gpio_3, 0x0, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_4] = RTK_PIN_CONFIG(iso_gpio_4, 0x0, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_5] = RTK_PIN_CONFIG(iso_gpio_5, 0x0, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_HDMI_HPD] = RTK_PIN_CONFIG(hdmi_hpd, 0xc, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_7] = RTK_PIN_CONFIG(iso_gpio_7, 0x0, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_IR_RX] = RTK_PIN_CONFIG(ir_rx, 0x0, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_IR_TX] = RTK_PIN_CONFIG(ir_tx, 0x8, 20, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_UR0_RX] = RTK_PIN_CONFIG(ur0_rx, 0x4, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_UR0_TX] = RTK_PIN_CONFIG(ur0_tx, 0x4, 20, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_UR1_RX] = RTK_PIN_CONFIG(ur1_rx, 0x4, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_UR1_TX] = RTK_PIN_CONFIG(ur1_tx, 0x4, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_UR1_CTS_N] = RTK_PIN_CONFIG(ur1_cts_n, 0x4, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_UR1_RTS_N] = RTK_PIN_CONFIG(ur1_rts_n, 0x4, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_I2C_SCL_0] = RTK_PIN_CONFIG(i2c_scl_0, 0x0, 28, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_I2C_SDA_0] = RTK_PIN_CONFIG(i2c_sda_0, 0x0, 24, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_I2C_SCL_1] = RTK_PIN_CONFIG(i2c_scl_1, 0x8, 28, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_I2C_SDA_1] = RTK_PIN_CONFIG(i2c_sda_1, 0x8, 24, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_I2C_SCL_6] = RTK_PIN_CONFIG(i2c_scl_6, 0x8, 16, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_ISO_GPIO_21] = RTK_PIN_CONFIG(iso_gpio_21, 0x18, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_22] = RTK_PIN_CONFIG(iso_gpio_22, 0x18, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_23] = RTK_PIN_CONFIG(iso_gpio_23, 0x18, 8, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_24] = RTK_PIN_CONFIG(iso_gpio_24, 0x18, 12, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_25] = RTK_PIN_CONFIG(iso_gpio_25, 0x18, 16, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_I2C_SDA_6] = RTK_PIN_CONFIG(i2c_sda_6, 0x8, 12, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_ETN_LED_LINK] = RTK_PIN_CONFIG(etn_led_link, 0x4, 24, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_ETN_LED_RXTX] = RTK_PIN_CONFIG(etn_led_rxtx, 0x4, 28, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_NAT_LED_0] = RTK_PIN_CONFIG(nat_led_0, 0xc, 16, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_NAT_LED_1] = RTK_PIN_CONFIG(nat_led_1, 0xc, 20, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_NAT_LED_2] = RTK_PIN_CONFIG(nat_led_2, 0xc, 24, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_NAT_LED_3] = RTK_PIN_CONFIG(nat_led_3, 0xc, 28, 1, 0, 2, 3, NA, PADDRI_4_8),
	[RTD1295_ISO_ISO_GPIO_33] = RTK_PIN_CONFIG(iso_gpio_33, 0x20, 0, 1, 0, 2, 3, NA, PADDRI_2_4),
	[RTD1295_ISO_ISO_GPIO_34] = RTK_PIN_CONFIG(iso_gpio_34, 0x20, 4, 1, 0, 2, 3, NA, PADDRI_2_4),
};

static const struct rtd_pinctrl_desc rtd1295_iso_pinctrl_desc = {
	.pins = rtd1295_iso_pins,
	.num_pins = ARRAY_SIZE(rtd1295_iso_pins),
	.groups = rtd1295_iso_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1295_iso_pin_groups),
	.functions = rtd1295_iso_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1295_iso_pin_functions),
	.muxes = rtd1295_iso_muxes,
	.num_muxes = ARRAY_SIZE(rtd1295_iso_muxes),
	.configs = rtd1295_iso_configs,
	.num_configs = ARRAY_SIZE(rtd1295_iso_configs),
};

static int rtd1295_pinctrl_probe(struct platform_device *pdev)
{
	const struct rtd_pinctrl_desc *desc = device_get_match_data(&pdev->dev);

	return rtd_pinctrl_probe(pdev, desc);
}

static const struct of_device_id rtd1295_pinctrl_of_match[] = {
	{ .compatible = "realtek,rtd1295-iso-pinctrl", .data = &rtd1295_iso_pinctrl_desc },
	{ .compatible = "realtek,rtd1295-sb2-pinctrl", .data = &rtd1295_sb2_pinctrl_desc },
	{ .compatible = "realtek,rtd1295-disp-pinctrl", .data = &rtd1295_disp_pinctrl_desc },
	{ .compatible = "realtek,rtd1295-cr-pinctrl", .data = &rtd1295_cr_pinctrl_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtd1295_pinctrl_of_match);

static struct platform_driver rtd1295_pinctrl_driver = {
	.driver = {
		.name = "rtd1295-pinctrl",
		.of_match_table = rtd1295_pinctrl_of_match,
		.pm = &realtek_pinctrl_pm_ops,
	},
	.probe = rtd1295_pinctrl_probe,
};

static int __init rtd1295_pinctrl_init(void)
{
	return platform_driver_register(&rtd1295_pinctrl_driver);
}
arch_initcall(rtd1295_pinctrl_init);

static void __exit rtd1295_pinctrl_exit(void)
{
	platform_driver_unregister(&rtd1295_pinctrl_driver);
}
module_exit(rtd1295_pinctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek RTD1295 pinctrl driver");
