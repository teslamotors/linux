/*
 * drivers/input/keyboard/tegra-kbc.c
 *
 * Keyboard class input driver for the NVIDIA Tegra SoC internal matrix
 * keyboard controller
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <mach/clk.h>
#include <mach/kbc.h>

#define KBC_CONTROL_0	0
#define KBC_INT_0	4
#define KBC_ROW_CFG0_0	8
#define KBC_COL_CFG0_0	0x18
#define KBC_RPT_DLY_0	0x2c
#define KBC_KP_ENT0_0	0x30
#define KBC_KP_ENT1_0	0x34
#define KBC_ROW0_MASK_0	0x38

#define DEBUG_MSG(m) printk(m)

#define res_size(res)	((res)->end - (res)->start + 1)

struct tegra_kbc {
	void __iomem *mmio;
	struct input_dev *idev;
	int irq;
	unsigned int wake_enable_rows;
	unsigned int wake_enable_cols;
	spinlock_t lock;
	unsigned int repoll_time;
	struct tegra_kbc_platform_data *pdata;
	int *plain_keycode;
	int *fn_keycode;
	struct work_struct key_repeat;
	struct workqueue_struct *kbc_work_queue;
	struct clk *clk;
};

static int plain_kbd_keycode[] = {
	/*
	 * Row 0 Unused, Unused, 'W', 'S', 'A', 'Z', Unused, Function,
	 * Row 1 Unused, Unused, Unused, Unused, Unused, Unused, Unused, Menu
	 * Row 2 Unused, Unused, Unused, Unused, Unused, Unused, Alt, Alt2
	 * Row 3 '5', '4', 'R', 'E', 'F', 'D', 'X', Unused,
	 * Row 4 '7', '6', 'T', 'H', 'G', 'V', 'C', SPACEBAR,
	 * Row 5 '9', '8', 'U', 'Y', 'J', 'N', 'B', '|\',
	 * Row 6 Minus, '0', 'O', 'I', 'L', 'K', '<', M,
	 * Row 7 Unused, '+', '}]', '#', Unused, Unused, Unused, Menu,
	 * Row 8 Unused, Unused, Unused, Unused, SHIFT, SHIFT, Unused, Unused,
	 * Row 9 Unused, Unused, Unused, Unused, Unused, Ctrl, Unused, Ctrl,
	 * Row A Unused, Unused, Unused, Unused, Unused, Unused, Unused, Unused,
	 * Row B '{[', 'P', '"', ':;', '/?, '>', Unused, Unused,
	 * Row C 'F10', 'F9', 'BckSpc', '3', '2', Up, Prntscr, Pause
	 * Row D INS, DEL, Unused, Pgup, PgDn, right, Down, Left,
	 * Row E F11, F12, F8, 'Q', F4, F3, '1', F7,
	 * Row F ESC, '~', F5, TAB, F1, F2, CAPLOCK, F6,
	 */
	KEY_RESERVED, KEY_RESERVED, KEY_W, KEY_S,
		KEY_A, KEY_Z, KEY_RESERVED, KEY_FN,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_MENU,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_LEFTALT, KEY_RIGHTALT,
	KEY_5, KEY_4, KEY_R, KEY_E,
		KEY_F, KEY_D, KEY_X, KEY_RESERVED,
	KEY_7, KEY_6, KEY_T, KEY_H,
		KEY_G, KEY_V, KEY_C, KEY_SPACE,
	KEY_9, KEY_8, KEY_U, KEY_Y,
		KEY_J, KEY_N, KEY_B, KEY_BACKSLASH,
	KEY_MINUS, KEY_0, KEY_O, KEY_I,
		KEY_L, KEY_K, KEY_COMMA, KEY_M,
	KEY_RESERVED, KEY_EQUAL, KEY_RIGHTBRACE, KEY_ENTER,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_MENU,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_LEFTCTRL, KEY_RESERVED, KEY_RIGHTCTRL,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_LEFTBRACE, KEY_P, KEY_APOSTROPHE, KEY_SEMICOLON,
		KEY_SLASH, KEY_DOT, KEY_RESERVED, KEY_RESERVED,
	KEY_F10, KEY_F9, KEY_BACKSPACE, KEY_3,
		KEY_2, KEY_UP, KEY_PRINT, KEY_PAUSE,
	KEY_INSERT, KEY_DELETE, KEY_RESERVED, KEY_PAGEUP,
		KEY_PAGEDOWN, KEY_RIGHT, KEY_DOWN, KEY_LEFT,
	KEY_F11, KEY_F12, KEY_F8, KEY_Q,
		KEY_F4, KEY_F3, KEY_1, KEY_F7,
	KEY_ESC, KEY_GRAVE, KEY_F5, KEY_TAB,
		KEY_F1, KEY_F2, KEY_CAPSLOCK, KEY_F6
};

static int fn_kbd_keycode[] = {
	/*
	 * Row 0 Unused, Unused, 'W', 'S', 'A', 'Z', Unused, Function,
	 * Row 1 Special, Unused, Unused, Unused, Unused, Unused, Unused, Menu
	 * Row 2 Unused, Unused, Unused, Unused, Unused, Unused, Alt, Alt2
	 * Row 3 '5', '4', 'R', 'E', 'F', 'D', 'X', Unused,
	 * Row 4 '7', '6', 'T', 'H', 'G', 'V', 'C', SPACEBAR,
	 * Row 5 '9', '8', 'U', 'Y', 'J', 'N', 'B', '|\',
	 * Row 6 Minus, '0', 'O', 'I', 'L', 'K', '<', M,
	 * Row 7 Unused, '+', '}]', '#', Unused, Unused, Unused, Menu,
	 * Row 8 Unused, Unused, Unused, Unused, SHIFT, SHIFT, Unused, Unused,
	 * Row 9 Unused, Unused, Unused, Unused, Unused, Ctrl, Unused, Control,
	 * Row A Unused, Unused, Unused, Unused, Unused, Unused, Unused, Unused,
	 * Row B '{[', 'P', '"', ':;', '/?, '>', Unused, Unused,
	 * Row C 'F10', 'F9', 'BckSpc', '3', '2', 'Up, Prntscr, Pause
	 * Row D INS, DEL, Unused, Pgup, PgDn, right, Down, Left,
	 * Row E F11, F12, F8, 'Q', F4, F3, '1', F7,
	 * Row F ESC, '~', F5, TAB, F1, F2, CAPLOCK, F6,
	 */
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_7, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_9, KEY_8, KEY_4, KEY_RESERVED,
		KEY_1, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_SLASH, KEY_6, KEY_5,
		KEY_3, KEY_2, KEY_RESERVED, KEY_0,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_KPASTERISK, KEY_RESERVED, KEY_KPMINUS,
		KEY_KPPLUS, KEY_DOT, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_VOLUMEUP, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_HOME,
		KEY_END, KEY_BRIGHTNESSUP, KEY_VOLUMEDOWN, KEY_BRIGHTNESSDOWN,
	KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_MUTE, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_QUESTION, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED
};

static int tegra_kbc_filter_keys(int *prows, int *pcols, int num_of_key_pressed)
{
	int i=0;
	int j=0;
	int k=0;
	int filter_keys[2] = {0};
	int is_filtered = false;
	int new_key_press_count = num_of_key_pressed;

	DEBUG_MSG("KBC: tegra_kbc_filter_keys\n");

	if (num_of_key_pressed <= 3)
	{
		for (i=0; i<num_of_key_pressed; i++)
		{
			for (j=(i+1); j<num_of_key_pressed; j++)
			{
				if ((prows[i]+1==prows[j])||(prows[j]+1==prows[i]))
				{
					for (k=j; i<(num_of_key_pressed - 1); i++)
					{
						prows[k] = prows[k+1];
						pcols[k] = pcols[k+1];
					}
					num_of_key_pressed--;
				}
				if ((pcols[i]+1==pcols[j])||(pcols[j]+1==pcols[i]))
				{
					for (k=j; i<(num_of_key_pressed - 1); i++)
					{
						prows[k] = prows[k+1];
						pcols[k] = pcols[k+1];
					}
					num_of_key_pressed--;
				}
			}
		}
		return num_of_key_pressed;
	}

	for (i=0; i<num_of_key_pressed; i++)
	{
		for (j=(i+1); j<num_of_key_pressed; j++)
		{
			if (prows[i] == prows[j])
			{
				for (k=0; k<num_of_key_pressed; k++)
				{
					if (k == i)
						continue;

					if(pcols[i] == pcols[k])
					{
						filter_keys[0] = k;
						is_filtered = true;
					}
				}
				for (k=0; k<num_of_key_pressed; k++)
				{
					if (k == j)
						continue;
					if (pcols[j] == pcols[k])
					{
						filter_keys[1] = k;
						is_filtered = true;
					}
				}
				goto end;
			}
		}
	}

	end:
	if (is_filtered)
	{
		for (i=filter_keys[0]; i<(num_of_key_pressed - 1); i++)
		{
			prows[i] = prows[i+1];
			pcols[i] = pcols[i+1];
		}
		new_key_press_count--;
		for (i=filter_keys[1]; i<(num_of_key_pressed - 1); i++)
		{
			prows[i] = prows[i+1];
			pcols[i] = pcols[i+1];
		}
		new_key_press_count--;
	}
	num_of_key_pressed = new_key_press_count;
	return new_key_press_count;
}


static int tegra_kbc_keycode(struct tegra_kbc *kbc, int r, int c, bool fn_key)
{
	if (!fn_key)
		return kbc->plain_keycode[(r * KBC_MAX_COL) + c];
	else
		return kbc->fn_keycode[(r * KBC_MAX_COL) + c];
}

#ifdef CONFIG_PM
static int tegra_kbc_open(struct input_dev *dev);
static void tegra_kbc_close(struct input_dev *dev);
static void tegra_kbc_setup_wakekeys(struct tegra_kbc *kbc, bool filter);

static int tegra_kbc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_kbc *kbc = platform_get_drvdata(pdev);

	DEBUG_MSG("KBC: tegra_kbc_suspend\n");

	if (device_may_wakeup(&pdev->dev)) {
		tegra_kbc_setup_wakekeys(kbc, true);
		enable_irq_wake(kbc->irq);
		/* Forcefully clear the interrupt status */
		writel(0x7, kbc->mmio + KBC_INT_0);
		msleep(30);
	} else {
		tegra_kbc_close(kbc->idev);
	}

	return 0;
}

static int tegra_kbc_resume(struct platform_device *pdev)
{
	struct tegra_kbc *kbc = platform_get_drvdata(pdev);

	DEBUG_MSG("KBC: tegra_kbc_resume\n");

	if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(kbc->irq);
		tegra_kbc_setup_wakekeys(kbc, false);
	} else if (kbc->idev->users)
		return tegra_kbc_open(kbc->idev);

	return 0;
}
#endif

static void tegra_kbc_report_keys(struct tegra_kbc *kbc, int *fifo)
{
	int curr_fifo[KBC_MAX_KPENT];
	int rows_val[KBC_MAX_KPENT], cols_val[KBC_MAX_KPENT];
	u32 kp_ent_val[(KBC_MAX_KPENT + 3) / 4];
	u32 *kp_ents = kp_ent_val;
	u32 kp_ent = 0;
	unsigned long flags;
	int i, j, valid = 0;
	bool fn = false;

	DEBUG_MSG("KBC: tegra_kbc_report_keys\n");

	local_irq_save(flags);
	for (i = 0; i < ARRAY_SIZE(kp_ent_val); i++)
		kp_ent_val[i] = readl(kbc->mmio + KBC_KP_ENT0_0 + (i*4));
	local_irq_restore(flags);

	valid = 0;
	for (i = 0; i < KBC_MAX_KPENT; i++) {
		if (!(i&3))
			kp_ent = *kp_ents++;

		if (kp_ent & 0x80) {
			cols_val[valid] = kp_ent & 0x7;
			rows_val[valid++] = (kp_ent >> 3) & 0xf;
		}
		kp_ent >>= 8;
	}

	if (kbc->pdata->filter_keys)
		valid = tegra_kbc_filter_keys(rows_val, cols_val, valid);

	for (i = 0; i < valid; i++) {
		int k = tegra_kbc_keycode(kbc, rows_val[i], cols_val[i], false);
		if (k == KEY_FN) {
			fn = true;
			break;
		}
	}

	j = 0;
	for (i = 0; i < valid; i++) {
		int k = tegra_kbc_keycode(kbc, rows_val[i], cols_val[i], fn);
		if (likely(k != -1))
			curr_fifo[j++] = k;
	}
	valid = j;

	for (i = 0; i < KBC_MAX_KPENT; i++) {
		if (fifo[i] == -1)
			continue;
		for (j = 0; j < valid; j++) {
			if (curr_fifo[j] == fifo[i]) {
				curr_fifo[j] = -1;
				break;
			}
		}
		if (j == valid) {
			input_report_key(kbc->idev, fifo[i], 0);
			fifo[i] = -1;
		}
	}
	for (j = 0; j < valid; j++) {
		if (curr_fifo[j] == -1)
			continue;
		for (i = 0; i < KBC_MAX_KPENT; i++) {
			if (fifo[i] == -1)
				break;
		}
		if (i != KBC_MAX_KPENT) {
			fifo[i] = curr_fifo[j];
			input_report_key(kbc->idev, fifo[i], 1);
		} else
			WARN_ON(1);
	}
}

static void tegra_kbc_key_repeat(struct work_struct *work)
{
	struct tegra_kbc *kbc;
	unsigned long flags;
	u32 val;
	int fifo[KBC_MAX_KPENT];
	int i;

	DEBUG_MSG("KBC: tegra_kbc_key_repeat\n");

	kbc = container_of(work, struct tegra_kbc, key_repeat);
	for (i = 0; i < ARRAY_SIZE(fifo); i++)
		fifo[i] = -1;

	while (1) {
		val = (readl(kbc->mmio + KBC_INT_0) >> 4) & 0xf;
		if (!val) {
			/* release any pressed keys and exit the loop */
			for (i = 0; i < ARRAY_SIZE(fifo); i++) {
				if (fifo[i] == -1)
					continue;
				input_report_key(kbc->idev, fifo[i], 0);
			}
			break;
		}
		tegra_kbc_report_keys(kbc, fifo);
		msleep((val == 1) ? kbc->repoll_time : 1);
	}

	spin_lock_irqsave(&kbc->lock, flags);
	val = readl(kbc->mmio + KBC_CONTROL_0);
	val |= (1<<3);
	writel(val, kbc->mmio + KBC_CONTROL_0);
	spin_unlock_irqrestore(&kbc->lock, flags);
}

static void tegra_kbc_close(struct input_dev *dev)
{
	struct tegra_kbc *kbc = input_get_drvdata(dev);
	unsigned long flags;
	u32 val;

	DEBUG_MSG("KBC: tegra_kbc_close\n");

	spin_lock_irqsave(&kbc->lock, flags);
	val = readl(kbc->mmio + KBC_CONTROL_0);
	val &= ~1;
	writel(val, kbc->mmio + KBC_CONTROL_0);
	spin_unlock_irqrestore(&kbc->lock, flags);

	clk_disable(kbc->clk);
}

static void tegra_kbc_setup_wakekeys(struct tegra_kbc *kbc, bool filter)
{
	int i;
	unsigned int rst_val;

	DEBUG_MSG("KBC: tegra_kbc_setup_wakekeys\n");

	BUG_ON(kbc->pdata->wake_cnt > KBC_MAX_KEY);
	rst_val = (filter && kbc->pdata->wake_cnt) ? ~0 : 0;

	for (i = 0; i < KBC_MAX_ROW; i++)
		writel(rst_val, kbc->mmio+KBC_ROW0_MASK_0+i*4);

	if (filter) {
		for (i = 0; i < kbc->pdata->wake_cnt; i++) {
			u32 val, addr;
			addr = kbc->pdata->wake_cfg[i].row*4 + KBC_ROW0_MASK_0;
			val = readl(kbc->mmio + addr);
			val &= ~(1<<kbc->pdata->wake_cfg[i].col);
			writel(val, kbc->mmio + addr);
		}
	}
}

static void tegra_kbc_config_pins(struct tegra_kbc *kbc)
{
	const struct tegra_kbc_platform_data *pdata = kbc->pdata;
	int i;

	DEBUG_MSG("KBC: tegra_kbc_config_pins\n");

	for (i = 0; i < KBC_MAX_GPIO; i++) {
		u32 row_cfg, col_cfg;
		u32 r_shift = 5 * (i%6);
		u32 c_shift = 4 * (i%8);
		u32 r_mask = 0x1f << r_shift;
		u32 c_mask = 0xf << c_shift;
		u32 r_offs = (i / 6) * 4 + KBC_ROW_CFG0_0;
		u32 c_offs = (i / 8) * 4 + KBC_COL_CFG0_0;

		row_cfg = readl(kbc->mmio + r_offs);
		col_cfg = readl(kbc->mmio + c_offs);

		row_cfg &= ~r_mask;
		col_cfg &= ~c_mask;

		if (pdata->pin_cfg[i].is_row)
			row_cfg |= ((pdata->pin_cfg[i].num<<1) | 1) << r_shift;
		else if (pdata->pin_cfg[i].is_col)
			col_cfg |= ((pdata->pin_cfg[i].num<<1) | 1) << c_shift;

		writel(row_cfg, kbc->mmio + r_offs);
		writel(col_cfg, kbc->mmio + c_offs);
	}
}

static int tegra_kbc_open(struct input_dev *dev)
{
	struct tegra_kbc *kbc = input_get_drvdata(dev);
	unsigned long flags;
	u32 val = 0;

	DEBUG_MSG("KBC: tegra_kbc_open\n");

	clk_enable(kbc->clk);

	/* Reset the KBC controller to clear all previous status.*/
	tegra_periph_reset_assert(kbc->clk);
	udelay(100);
	tegra_periph_reset_deassert(kbc->clk);
	udelay(100);

	tegra_kbc_config_pins(kbc);
	tegra_kbc_setup_wakekeys(kbc, false);

	writel(kbc->pdata->repeat_cnt, kbc->mmio + KBC_RPT_DLY_0);

	val = kbc->pdata->debounce_cnt << 4;
	val |= 1<<14; /* fifo interrupt threshold = 1 entry */
	val |= 1<<3;  /* interrupt on FIFO threshold reached */
	val |= 1;     /* enable */
	writel(val, kbc->mmio + KBC_CONTROL_0);

	/* atomically clear out any remaining entries in the key FIFO
	 * and enable keyboard interrupts */
	spin_lock_irqsave(&kbc->lock, flags);

	while (1) {
		val = readl(kbc->mmio + KBC_INT_0);
		val >>= 4;
		if (val) {
			val = readl(kbc->mmio + KBC_KP_ENT0_0);
			val = readl(kbc->mmio + KBC_KP_ENT1_0);
		} else {
			break;
		}
	}
	writel(0x7, kbc->mmio + KBC_INT_0);
	spin_unlock_irqrestore(&kbc->lock, flags);


	return 0;
}


static int __devexit tegra_kbc_remove(struct platform_device *pdev)
{
	struct tegra_kbc *kbc = platform_get_drvdata(pdev);
	struct resource *res;

	DEBUG_MSG("KBC: tegra_kbc_remove\n");

	free_irq(kbc->irq, pdev);
	clk_disable(kbc->clk);
	clk_put(kbc->clk);

	input_unregister_device(kbc->idev);
	input_free_device(kbc->idev);
	iounmap(kbc->mmio);
	destroy_workqueue(kbc->kbc_work_queue);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, res_size(res));

	kfree(kbc);
	return 0;
}

static irqreturn_t tegra_kbc_isr(int irq, void *args)
{
	struct tegra_kbc *kbc = args;
	u32 val, ctl;

	DEBUG_MSG("KBC: tegra_kbc_isr\n");

	/* until all keys are released, defer further processing to
	 * the polling loop in tegra_kbc_key_repeat */
	ctl = readl(kbc->mmio + KBC_CONTROL_0);
	ctl &= ~(1<<3);
	writel(ctl, kbc->mmio + KBC_CONTROL_0);

	/* quickly bail out & reenable interrupts if the interrupt source
	 * wasn't fifo count threshold */
	val = readl(kbc->mmio + KBC_INT_0);
	writel(val, kbc->mmio + KBC_INT_0);

	if (!(val & (1<<2))) {
		ctl |= 1<<3;
		writel(ctl, kbc->mmio + KBC_CONTROL_0);
		return IRQ_HANDLED;
	}

	queue_work(kbc->kbc_work_queue, &kbc->key_repeat);
	return IRQ_HANDLED;
}

static int __devinit tegra_kbc_probe(struct platform_device *pdev)
{
	struct tegra_kbc *kbc;
	struct tegra_kbc_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	int irq;
	int err;
	int rows[KBC_MAX_ROW];
	int cols[KBC_MAX_COL];
	int i, j;
	int nr = 0;
	char name[64];

	DEBUG_MSG("KBC: tegra_kbc_probe\n");

	if (!pdata)
		return -EINVAL;

	kbc = kzalloc(sizeof(*kbc), GFP_KERNEL);
	if (!kbc)
		return -ENOMEM;

	kbc->pdata = pdata;
	kbc->irq = -EINVAL;

	memset(rows, 0, sizeof(rows));
	memset(cols, 0, sizeof(cols));

	kbc->idev = input_allocate_device();
	if (!kbc->idev) {
		err = -ENOMEM;
		goto fail;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get I/O memory\n");
		err = -ENXIO;
		goto fail;
	}
	res = request_mem_region(res->start, res_size(res), pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "failed to request I/O memory\n");
		err = -EBUSY;
		goto fail;
	}
	kbc->mmio = ioremap(res->start, res_size(res));
	if (!kbc->mmio) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		err = -ENXIO;
		goto fail;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get keypad IRQ\n");
		err = -ENXIO;
		goto fail;
	}
	kbc->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR_OR_NULL(kbc->clk)) {
		dev_err(&pdev->dev, "failed to get keypad clock\n");
		err = (kbc->clk) ? PTR_ERR(kbc->clk) : -ENODEV;
		kbc->clk = NULL;
		goto fail;
	}

	platform_set_drvdata(pdev, kbc);

	kbc->idev->name = pdev->name;
	input_set_drvdata(kbc->idev, kbc);
	kbc->idev->id.bustype = BUS_HOST;
	kbc->idev->open = tegra_kbc_open;
	kbc->idev->close = tegra_kbc_close;
	kbc->idev->dev.parent = &pdev->dev;
	spin_lock_init(&kbc->lock);

	for (i = 0; i < KBC_MAX_GPIO; i++) {
		if (pdata->pin_cfg[i].is_row && pdata->pin_cfg[i].is_col) {
			dev_err(&pdev->dev, "invalid pin configuration data\n");
			err = -EINVAL;
			goto fail;
		}

		if (pdata->pin_cfg[i].is_row) {
			if (pdata->pin_cfg[i].num >= KBC_MAX_ROW) {
				dev_err(&pdev->dev, "invalid row number\n");
				err = -EINVAL;
				goto fail;
			}
			rows[pdata->pin_cfg[i].num] = 1;
			nr++;
		} else if (pdata->pin_cfg[i].is_col) {
			if (pdata->pin_cfg[i].num >= KBC_MAX_COL) {
				dev_err(&pdev->dev, "invalid column number\n");
				err = -EINVAL;
				goto fail;
			}
			cols[pdata->pin_cfg[i].num] = 1;
		}
	}
	kbc->wake_enable_rows = 0;
	kbc->wake_enable_cols = 0;

	for (i = 0; i < pdata->wake_cnt; i++) {
		kbc->wake_enable_rows |= (1 << kbc->pdata->wake_cfg[i].row);
		kbc->wake_enable_cols |= (1 << kbc->pdata->wake_cfg[i].col);
	}

	pdata->debounce_cnt = min_t(unsigned int, pdata->debounce_cnt, 0x3fful);
	kbc->repoll_time = 5 + (16+pdata->debounce_cnt)*nr + pdata->repeat_cnt;
	kbc->repoll_time = (kbc->repoll_time + 31) / 32;

	kbc->idev->evbit[0] = BIT_MASK(EV_KEY);

	/* Override the default keycodes with the board supplied ones. */
	if (pdata->plain_keycode) {
		kbc->plain_keycode = pdata->plain_keycode;
		kbc->fn_keycode = pdata->fn_keycode;
	} else {
		kbc->plain_keycode = plain_kbd_keycode;
		kbc->fn_keycode = fn_kbd_keycode;
	}

	for (i = 0; i < KBC_MAX_COL; i++) {
		if (!cols[i])
			continue;
		for (j = 0; j < KBC_MAX_ROW; j++) {
			int keycode;
			if (!rows[j])
				continue;
			keycode = tegra_kbc_keycode(kbc, j, i, false);
			if (keycode == -1)
				continue;
			set_bit(keycode, kbc->idev->keybit);
		}
	}


	/* create the workqueue for the kbc path */
	snprintf(name, sizeof(name), "tegra-kbc");
	kbc->kbc_work_queue = create_singlethread_workqueue(name);
	if (kbc->kbc_work_queue == NULL) {
		dev_err(&pdev->dev, "Failed to create work queue\n");
		err = -ENODEV;
		goto fail;
	}

	/* keycode FIFO needs to be read atomically; leave local
	 * interrupts disabled when handling KBC interrupt */
	INIT_WORK(&kbc->key_repeat, tegra_kbc_key_repeat);

	err = request_irq(irq, tegra_kbc_isr, IRQF_DISABLED, pdev->name, kbc);
	if (err) {
		dev_err(&pdev->dev, "failed to request keypad IRQ\n");
		goto fail;
	}
	kbc->irq = irq;

	err = input_register_device(kbc->idev);
	if (err) {
		dev_err(&pdev->dev, "failed to register input device\n");
		goto fail;
	}

	device_init_wakeup(&pdev->dev, 1);
	return 0;

fail:
	if (kbc->irq >= 0)
		free_irq(kbc->irq, pdev);
	if (kbc->idev)
		input_free_device(kbc->idev);
	if (kbc->clk)
		clk_put(kbc->clk);
	if (kbc->mmio)
		iounmap(kbc->mmio);
	if (kbc->kbc_work_queue)
		destroy_workqueue(kbc->kbc_work_queue);
	kfree(kbc);

	return err;
}

static struct platform_driver tegra_kbc_driver = {
	.probe   = tegra_kbc_probe,
	.remove  = tegra_kbc_remove,
#ifdef CONFIG_PM
	.suspend = tegra_kbc_suspend,
	.resume  = tegra_kbc_resume,
#endif
	.driver = {
		.name = "tegra-kbc"
	}
};

static void __exit tegra_kbc_exit(void)
{
	DEBUG_MSG("KBC: tegra_kbc_exit\n");
	platform_driver_unregister(&tegra_kbc_driver);
}

static int __devinit tegra_kbc_init(void)
{
	DEBUG_MSG("KBC: tegra_kbc_init\n");
	return platform_driver_register(&tegra_kbc_driver);
}

module_exit(tegra_kbc_exit);
module_init(tegra_kbc_init);

MODULE_DESCRIPTION("Tegra matrix keyboard controller driver");


