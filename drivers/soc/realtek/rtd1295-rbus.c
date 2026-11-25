// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 A01 rbus erratum lock
 *
 * On RTD129x A01, eMMC register accesses can be corrupted by concurrent CPU
 * accesses of other rbus masters to offsets 0x800-0x9ff of a 4K page, such
 * as Wi-Fi card registers reached through the PCIe controller's window.
 * The vendor kernel serialised them with one global spinlock (rtk_lockapi);
 * this is that lock, shared by the drivers concerned.
 */

#include <linux/export.h>
#include <linux/soc/realtek/rtd1295-rbus.h>
#include <linux/spinlock.h>

static DEFINE_RAW_SPINLOCK(rtd1295_rbus_raw_lock);

void rtd1295_rbus_lock(unsigned long *flags)
{
	raw_spin_lock_irqsave(&rtd1295_rbus_raw_lock, *flags);
}
EXPORT_SYMBOL_GPL(rtd1295_rbus_lock);

void rtd1295_rbus_unlock(unsigned long flags)
{
	raw_spin_unlock_irqrestore(&rtd1295_rbus_raw_lock, flags);
}
EXPORT_SYMBOL_GPL(rtd1295_rbus_unlock);
