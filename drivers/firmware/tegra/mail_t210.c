/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <mach/irqs.h>
#include "../../../arch/arm/mach-tegra/iomap.h"
#include "bpmp.h"

/* CPU to BPMP atomic channels */
#define CPU0_OB_CH0		0
#define CPU1_OB_CH0		1
#define CPU2_OB_CH0		2
#define CPU3_OB_CH0		3

/* CPU to BPMP non-atomic channels */
#define CPU0_OB_CH1		4
#define CPU1_OB_CH1		5
#define CPU2_OB_CH1		6
#define CPU3_OB_CH1		7

/* BPMP to CPU channels */
#define CPU0_IB_CH		8
#define CPU1_IB_CH		9
#define CPU2_IB_CH		10
#define CPU3_IB_CH		11

#define CPU_OB_IRQ		INT_SHR_SEM_OUTBOX_IBF
#define CPU0_IB_IRQ		INT_SHR_SEM_INBOX_IBF
#define CPU1_IB_IRQ		INT_SHR_SEM_INBOX_IBE
#define CPU2_IB_IRQ		INT_SHR_SEM_OUTBOX_IBE
#define CPU3_IB_IRQ		INT_AVP_UCQ

#define TEGRA_ATOMICS_BASE	0x70016000
#define ATOMICS_AP0_TRIGGER	IO_ADDRESS(TEGRA_ATOMICS_BASE + 0x000)
#define ATOMICS_AP0_RESULT(id)	IO_ADDRESS(TEGRA_ATOMICS_BASE + 0xc00 + id * 4)
#define TRIGGER_ID_SHIFT	16
#define TRIGGER_CMD_GET		4

#define ICTLR_REG_BASE(irq)	IO_ADDRESS(TEGRA_PRIMARY_ICTLR_BASE + (((irq) - 32) >> 5) * 0x100)
#define ICTLR_FIR_SET(irq)	(ICTLR_REG_BASE(irq) + 0x18)
#define ICTLR_FIR_CLR(irq)	(ICTLR_REG_BASE(irq) + 0x1c)
#define FIR_BIT(irq)		(1 << ((irq) & 0x1f))

#define RES_SEMA_SHRD_SMP_STA	IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x00)
#define RES_SEMA_SHRD_SMP_SET	IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x04)
#define RES_SEMA_SHRD_SMP_CLR	IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x08)

#define PER_CPU_IB_CH(i)	(CPU0_IB_CH + i)

/*
 * How the token bits are interpretted
 *
 * SL_SIGL (b00): slave ch in signalled state
 * SL_QUED (b01): slave ch is in queue
 * MA_FREE (b10): master ch is free
 * MA_ACKD (b11): master ch is acked
 *
 * Ideally, the slave should only set bits while the
 * master do only clear them. But there is an exception -
 * see bpmp_ack_master()
 */
#define CH_MASK(ch)	(0x3 << ((ch) * 2))
#define SL_SIGL(ch)	(0x0 << ((ch) * 2))
#define SL_QUED(ch)	(0x1 << ((ch) * 2))
#define MA_FREE(ch)	(0x2 << ((ch) * 2))
#define MA_ACKD(ch)	(0x3 << ((ch) * 2))

static u32 bpmp_ch_sta(int ch)
{
	return __raw_readl(RES_SEMA_SHRD_SMP_STA) & CH_MASK(ch);
}

bool bpmp_master_free(int ch)
{
	return bpmp_ch_sta(ch) == MA_FREE(ch);
}

bool bpmp_slave_signalled(int ch)
{
	return bpmp_ch_sta(ch) == SL_SIGL(ch);
}

bool bpmp_master_acked(int ch)
{
	return bpmp_ch_sta(ch) == MA_ACKD(ch);
}

void bpmp_signal_slave(int ch)
{
	__raw_writel(CH_MASK(ch), RES_SEMA_SHRD_SMP_CLR);
}

static void bpmp_ack_master(int ch, int flags)
{
	__raw_writel(MA_ACKD(ch), RES_SEMA_SHRD_SMP_SET);

	if (flags & DO_ACK)
		return;

	/*
	 * We have to violate the bit modification rule while
	 * moving from SL_QUED to MA_FREE (DO_ACK not set) so that
	 * the channel won't be in ACKD state forever.
	 */
	__raw_writel(MA_ACKD(ch) ^ MA_FREE(ch), RES_SEMA_SHRD_SMP_CLR);
}

/* MA_ACKD to MA_FREE */
void bpmp_free_master(int ch)
{
	__raw_writel(MA_ACKD(ch) ^ MA_FREE(ch), RES_SEMA_SHRD_SMP_CLR);
}

void bpmp_ring_doorbell(int ch)
{
	writel(FIR_BIT(CPU_OB_IRQ), ICTLR_FIR_SET(CPU_OB_IRQ));
}

static void bpmp_ack_doorbell(int irq)
{
	writel(FIR_BIT(irq), ICTLR_FIR_CLR(irq));
}

void tegra_bpmp_mail_return_data(int ch, int code, void *data, int sz)
{
	struct mb_data *p;
	int flags;

	if (sz > MSG_DATA_SZ) {
		WARN_ON(1);
		return;
	}

	p = channel_area[ch].ob;
	p->code = code;
	memcpy(p->data, data, sz);

	flags = channel_area[ch].ib->flags;
	bpmp_ack_master(ch, flags);
	if (flags & RING_DOORBELL)
		bpmp_ring_doorbell(ch);
}
EXPORT_SYMBOL(tegra_bpmp_mail_return_data);

int bpmp_thread_ch_index(int ch)
{
	if (ch < CPU0_OB_CH1 || ch > CPU3_OB_CH1)
		return -1;
	return ch - CPU0_OB_CH1;
}

int bpmp_thread_ch(int idx)
{
	return CPU0_OB_CH1 + idx;
}

int bpmp_ob_channel(void)
{
	return smp_processor_id() + CPU0_OB_CH0;
}

static int cpu_irqs[] = { CPU0_IB_IRQ, CPU1_IB_IRQ, CPU2_IB_IRQ, CPU3_IB_IRQ };

static void bpmp_irq_set_affinity(int cpu)
{
	int nr_cpus = num_present_cpus();
	int r;
	int i;

	for (i = cpu; i < ARRAY_SIZE(cpu_irqs); i += nr_cpus) {
		r = irq_set_affinity(cpu_irqs[i], cpumask_of(cpu));
		WARN_ON(r);
	}
}

static void bpmp_irq_clr_affinity(int cpu)
{
	int nr_cpus = num_present_cpus();
	int new_cpu;
	int r;
	int i;

	for (i = cpu; i < ARRAY_SIZE(cpu_irqs); i += nr_cpus) {
		new_cpu = cpumask_any_but(cpu_online_mask, cpu);
		r = irq_set_affinity(cpu_irqs[i], cpumask_of(new_cpu));
		WARN_ON(r);
	}
}

/*
 * When a CPU is being hot unplugged, the incoming
 * doorbell irqs must be moved to another CPU
 */
static int bpmp_cpu_notify(struct notifier_block *nb, unsigned long action,
		void *data)
{
	int cpu = (long)data;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		bpmp_irq_clr_affinity(cpu);
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		bpmp_irq_set_affinity(cpu);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bpmp_cpu_nb = {
	.notifier_call = bpmp_cpu_notify
};

static irqreturn_t bpmp_inbox_irq(int irq, void *data)
{
	int ch = (long)data;
	bpmp_ack_doorbell(irq);
	bpmp_handle_irq(ch);
	return IRQ_HANDLED;
}

int bpmp_init_irq(void)
{
	long ch;
	int r;
	int i;

	for (i = 0; i < ARRAY_SIZE(cpu_irqs); i++) {
		ch = PER_CPU_IB_CH(i);
		r = request_irq(cpu_irqs[i], bpmp_inbox_irq,
				0, "bpmp", (void *)ch);
		if (r)
			return r;
	}

	r = register_cpu_notifier(&bpmp_cpu_nb);
	if (r)
		return r;

	for_each_present_cpu(i)
		bpmp_irq_set_affinity(i);

	return 0;
}

/* Channel area is setup by BPMP before signalling handshake */
static void *bpmp_channel_area(int ch)
{
	u32 a;
	writel(ch << TRIGGER_ID_SHIFT | TRIGGER_CMD_GET, ATOMICS_AP0_TRIGGER);
	a = readl(ATOMICS_AP0_RESULT(ch));
	return a ? IO_ADDRESS(a) : NULL;
}

static int __bpmp_connect(void)
{
	void *p;
	int i;

	if (connected)
		return 0;

	/* handshake */
	if (!readl(RES_SEMA_SHRD_SMP_STA))
		return -ENODEV;

	for (i = 0; i < NR_CHANNELS; i++) {
		p = bpmp_channel_area(i);
		if (!p)
			return -EFAULT;
		channel_area[i].ib = p;
		channel_area[i].ob = p;
	}

	connected = 1;
	return 0;
}

int bpmp_connect(void)
{
	/* firmware loaded after boot */
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC))
		return 0;

	return __bpmp_connect();
}

void bpmp_detach(void)
{
	int i;

	connected = 0;
	writel(0xffffffff, RES_SEMA_SHRD_SMP_CLR);

	for (i = 0; i < NR_CHANNELS; i++) {
		channel_area[i].ib = NULL;
		channel_area[i].ob = NULL;
	}
}

int bpmp_attach(void)
{
	int i;

	WARN_ON(connected);

	for (i = 0; i < MSEC_PER_SEC * 60; i += 20) {
		if (!__bpmp_connect())
			return 0;
		msleep(20);
	}

	return -ETIMEDOUT;
}

void tegra_bpmp_resume(void)
{
}

int bpmp_mail_init_prepare(void)
{
	return 0;
}
