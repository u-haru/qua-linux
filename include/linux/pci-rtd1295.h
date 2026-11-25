/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Realtek RTD1295 PCIe: device MMIO through the controller's 4K window.
 *
 * The CPU cannot reach card registers through the PCI MEM window on
 * RTD129x; drivers of devices behind this controller must use these
 * accessors (offset relative to BAR 2) instead of readl()/writel().
 */
#ifndef _LINUX_PCI_RTD1295_H
#define _LINUX_PCI_RTD1295_H

#include <linux/types.h>

struct pci_dev;

#if IS_ENABLED(CONFIG_PCIE_RTD1295)
bool rtd1295_pcie_is_rtd(struct pci_dev *pdev);
u32 rtd1295_pcie_mmio_read(struct pci_dev *pdev, u32 offset, int size);
void rtd1295_pcie_mmio_write(struct pci_dev *pdev, u32 offset, u32 val,
			     int size);
#else
static inline bool rtd1295_pcie_is_rtd(struct pci_dev *pdev)
{
	return false;
}

static inline u32 rtd1295_pcie_mmio_read(struct pci_dev *pdev, u32 offset,
					 int size)
{
	return ~0U;
}

static inline void rtd1295_pcie_mmio_write(struct pci_dev *pdev, u32 offset,
					   u32 val, int size)
{
}
#endif

#endif /* _LINUX_PCI_RTD1295_H */
