#ifndef __ARCH_ARM64_FIQ_H
#define __ARCH_ARM64_FIQ_H

#include <linux/irq.h>
struct fiq_chip {
	void (*fiq_enable)(struct irq_data *data);
	void (*fiq_disable)(struct irq_data *data);

	/* .fiq_ack() and .fiq_eoi() will be called from the FIQ
	 * handler. For this reason they must not use spin locks (or any
	 * other locks).
	 */
	int (*fiq_ack)(struct irq_data *data);
	void (*fiq_eoi)(struct irq_data *data);
};

static inline void fiq_register_mapping(int irq, struct fiq_chip *chip)
{
}

#endif /* __ARCH_ARM64_FIQ_H */
