/*
 *  linux/include/linux/mtd/nand_benand.h
 *
 *  (C) Copyright Toshiba Corporation
 *                Semiconductor and Storage Products Company 2015
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef __MTD_NAND_BENAND_H__
#define __MTD_NAND_BENAND_H__

#if defined(CONFIG_MTD_NAND_BENAND_ENABLE)

static inline int mtd_nand_has_benand(void) { return 1; }

void nand_benand_init(struct mtd_info *mtd);

int nand_read_page_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint8_t *buf, int oob_required, int page);

int nand_read_subpage_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint32_t data_offs, uint32_t readlen, uint8_t *bufpoi,
			int page);

#else  /* CONFIG_MTD_NAND_BENAND_ENABLE */
static inline int mtd_nand_has_benand(void) { return 0; }

void nand_benand_init(struct mtd_info *mtd) {}

int nand_read_page_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint8_t *buf, int oob_required, int page)
{
	return -1;
}

int nand_read_subpage_benand(struct mtd_info *mtd, struct nand_chip *chip,
			uint32_t data_offs, uint32_t readlen, uint8_t *bufpoi,
			int page)
{
	return -1;
}

#endif /* CONFIG_MTD_NAND_BENAND_ENABLE */
#endif
