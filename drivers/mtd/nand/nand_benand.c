/*
 *  drivers/mtd/nand_benand.c
 *
 *  (C) Copyright Toshiba Corporation
 *                Semiconductor and Storage Products Company 2015
 *
 *  This program supports BENAND.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *
 */

#include <linux/module.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_benand.h>

/* Recommended to rewrite for BENAND */
#define NAND_STATUS_RECOM_REWRT	0x08

/* ECC Status Read Command */
#define NAND_CMD_ECC_STATUS	0x7A

static struct nand_ecclayout benand_oob_64 = {
	.eccbytes = 0,
	.eccpos = {},
	.oobfree = {
		{.offset = 2, .length = 62}
	}
};

static struct nand_ecclayout benand_oob_128 = {
	.eccbytes = 0,
	.eccpos = {},
	.oobfree = {
		{.offset = 2, .length = 126}
	}
};


static int nand_benand_status_chk(struct mtd_info *mtd, struct nand_chip *chip)
{
	unsigned int max_bitflips = 0;
	u8 status;

	/* Check Read Status */
	chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
	status = chip->read_byte(mtd);

	/* timeout */
	if (!(status & NAND_STATUS_READY)) {
		pr_debug("BENAND : Time Out!\n");
		return -EIO;
	}

	/* uncorrectable */
	else if (status & NAND_STATUS_FAIL)
		mtd->ecc_stats.failed++;

	/* correctable */
	else if (status & NAND_STATUS_RECOM_REWRT) {
		if (chip->cmd_ctrl &&
			IS_ENABLED(CONFIG_MTD_NAND_BENAND_ECC_STATUS)) {

			int i;
			u8 ecc_status;
			unsigned int bitflips;

			/* Check Read ECC Status */
			chip->cmd_ctrl(mtd, NAND_CMD_ECC_STATUS,
				NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
			/* Get bitflips info per 512Byte */
			for (i = 0; i < mtd->writesize >> 9; i++) {
				ecc_status = chip->read_byte(mtd);
				bitflips = ecc_status & 0x0f;
				max_bitflips = max_t(unsigned int,
						max_bitflips, bitflips);
			}
			mtd->ecc_stats.corrected += max_bitflips;
		} else {
			/*
			 * If can't use chip->cmd_ctrl,
			 * we can't get real number of bitflips.
			 * So, we set max_bitflips mtd->bitflip_threshold.
			 */
			max_bitflips = mtd->bitflip_threshold;
			mtd->ecc_stats.corrected += max_bitflips;
		}
	}

	return max_bitflips;
}


void nand_benand_init(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	chip->options |= NAND_SUBPAGE_READ;

	switch (mtd->oobsize) {
	case 64:
		chip->ecc.layout = &benand_oob_64;
		break;
	case 128:
		chip->ecc.layout = &benand_oob_128;
		break;
	default:
		pr_warn("No oob scheme defined for oobsize %d\n",
				mtd->oobsize);
		BUG();
	}

}
EXPORT_SYMBOL(nand_benand_init);

int nand_read_page_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint8_t *buf, int oob_required, int page)
{
	unsigned int max_bitflips = 0;

	chip->ecc.read_page_raw(mtd, chip, buf, oob_required, page);
	max_bitflips = nand_benand_status_chk(mtd, chip);

	return max_bitflips;
}
EXPORT_SYMBOL(nand_read_page_benand);


int nand_read_subpage_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint32_t data_offs, uint32_t readlen, uint8_t *bufpoi,
			int page)
{
	uint8_t *p;
	unsigned int max_bitflips = 0;

	if (data_offs != 0)
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, data_offs, -1);

	p = bufpoi + data_offs;
	chip->read_buf(mtd, p, readlen);

	max_bitflips = nand_benand_status_chk(mtd, chip);

	return max_bitflips;
}
EXPORT_SYMBOL(nand_read_subpage_benand);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Toshiba Corporation Semiconductor and Storage Products Company");
MODULE_DESCRIPTION("BENAND (Built-in ECC NAND) support");
