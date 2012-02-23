#ifndef ZC_H
# define ZC_H

#include "cryptodev_int.h"

/* For zero copy */
int __get_userbuf(uint8_t __user *addr, uint32_t len, int write,
		int pgcount, struct page **pg, struct scatterlist *sg,
		struct task_struct *task, struct mm_struct *mm);
void release_user_pages(struct csession* ses);

int get_userbuf(struct csession *ses, void* __user src, int src_len,
                void* __user dst, int dst_len,
		struct task_struct *task, struct mm_struct *mm,
                struct scatterlist **src_sg, 
                struct scatterlist **dst_sg);

/* last page - first page + 1 */
#define PAGECOUNT(buf, buflen) \
	((((unsigned long)(buf + buflen - 1) & PAGE_MASK) >> PAGE_SHIFT) - \
	(((unsigned long) buf               & PAGE_MASK) >> PAGE_SHIFT) + 1)

#define DEFAULT_PREALLOC_PAGES 32

#endif
