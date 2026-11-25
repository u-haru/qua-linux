/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LINUX_SOC_REALTEK_RTD1295_RBUS_H
#define __LINUX_SOC_REALTEK_RTD1295_RBUS_H

/* irqsave semantics; see drivers/soc/realtek/rtd1295-rbus.c */
void rtd1295_rbus_lock(unsigned long *flags);
void rtd1295_rbus_unlock(unsigned long flags);

#endif
