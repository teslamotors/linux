#ifndef _ABL_CMDLINE_H_
#define _ABL_CMDLINE_H_

/**
 * memcpy_from_ph() - Copy block of data from physical memory.
 *
 * @dest:  Destination address.
 * @pp:    Address in physical memory.
 * @count: Number of bytes to copy.
 *
 * Return: 0 if OK or negative error code (see errno).
 */
int memcpy_from_ph(void *dest, phys_addr_t p, size_t count);

/**
 * memset_ph() - Copy block of data from physical memory.
 *
 * @pp:    Address in physical memory.
 * @val:   Value to be assigned.
 * @count: Number of bytes to copy.
 *
 * Return: 0 if OK or negative error code (see errno).
 */
int memset_ph(phys_addr_t p, int val, size_t count);

#endif /* _ABL_CMDLINE_H_ */
