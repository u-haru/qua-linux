// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTD1295 spin-table boot method.
 *
 * The firmware (BL31 v1.2 + OP-TEE) has no PSCI. All secondaries are parked
 * at EL3 polling one shared 32-bit SB2 register (cpu-release-addr). Writing
 * the physical address of rtd1295_secondary_trampoline there and issuing SEV
 * releases every secondary at once; the trampoline drops to EL2 and then waits
 * in the generic spin-table holding pen until its MPIDR is released.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/types.h>

#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/sysreg.h>

extern void rtd1295_secondary_trampoline(void);
extern volatile unsigned long secondary_holding_pen_release;

/* Read by the trampoline with the MMU off, before dropping from EL3 */
u64 __section(".mmuoff.data.read") rtd1295_secondary_cntfrq;

static phys_addr_t cpu_release_addr[NR_CPUS];

static void clean_to_poc(volatile void *p, size_t size)
{
	unsigned long start = (unsigned long)p;

	dcache_clean_inval_poc(start, start + size);
}

static int rtd1295_cpu_init(unsigned int cpu)
{
	struct device_node *dn;
	int ret;

	dn = of_get_cpu_node(cpu, NULL);
	if (!dn)
		return -ENODEV;

	ret = of_property_read_u64(dn, "cpu-release-addr",
				   &cpu_release_addr[cpu]);
	if (ret)
		pr_err("CPU %u: missing or invalid cpu-release-addr property\n",
		       cpu);

	of_node_put(dn);

	return ret;
}

static int rtd1295_cpu_prepare(unsigned int cpu)
{
	phys_addr_t pa = __pa_symbol(rtd1295_secondary_trampoline);
	void __iomem *release_addr;

	if (!cpu_release_addr[cpu])
		return -ENODEV;

	/* The release register is 32 bits wide */
	if (WARN_ON(upper_32_bits(pa)))
		return -EINVAL;

	rtd1295_secondary_cntfrq = read_sysreg(cntfrq_el0);
	clean_to_poc(&rtd1295_secondary_cntfrq,
		     sizeof(rtd1295_secondary_cntfrq));

	/*
	 * The register is shared by all CPUs: every secondary jumps to the
	 * trampoline on the first SEV and then waits in the holding pen, so
	 * writing it again for the other CPUs is harmless.
	 */
	release_addr = ioremap(cpu_release_addr[cpu], sizeof(u32));
	if (!release_addr)
		return -ENOMEM;

	writel_relaxed(lower_32_bits(pa), release_addr);
	dsb(sy);
	sev();

	iounmap(release_addr);

	return 0;
}

static int rtd1295_cpu_boot(unsigned int cpu)
{
	secondary_holding_pen_release = cpu_logical_map(cpu);
	clean_to_poc(&secondary_holding_pen_release,
		     sizeof(secondary_holding_pen_release));
	sev();

	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
static int rtd1295_cpu_disable(unsigned int cpu)
{
	return -EOPNOTSUPP;
}
#endif

const struct cpu_operations rtd1295_spin_table_ops = {
	.name		= "realtek,rtd1295-spin-table",
	.cpu_init	= rtd1295_cpu_init,
	.cpu_prepare	= rtd1295_cpu_prepare,
	.cpu_boot	= rtd1295_cpu_boot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= rtd1295_cpu_disable,
#endif
};
