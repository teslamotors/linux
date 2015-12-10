#ifndef _ABL_CMDLINE_H_
#define _ABL_CMDLINE_H_

/**
 * struct key_offset - ABL key address and size holder
 * @ksize: Size of RSA keys
 * @ikey:  Offset for Intel RSA public key
 * @okey:  Offset for OEM RSA public key
 */
struct key_offset {
	unsigned int	ksize;
	unsigned long	ikey;
	unsigned long	okey;
};

/**
 * struct seed_offset - ABL SEED address holder
 * @seed:  Array holding the SEED offset locations
 */
struct seed_offset {
	unsigned int	device_seed;
	unsigned int	user_seed;
};

/**
 * get_seed_offsets() - Get the SEED offsets from the kernel command line.
 * @param ksoff - output variable which returns all the offsets.
 *
 * Parse kernel cmdline string and returns the address of the
 * SEED values.
 *
 * Returns: 0 if OK or negative error code (see errno.h).
 */
int get_seed_offsets(struct seed_offset *ksoff);

/**
 * get_seckey_offsets() - Get the public key address from the  command line.
 * @param ksoff - output variable which returns all the offsets.
 *
 * Parse kernel cmdline string and returns the address and length of
 * the OEM and Intel public keys.
 *
 * Return: 0 if OK or negative error code (see errno.h).
 */
int get_seckey_offsets(struct key_offset *ksoff);

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
