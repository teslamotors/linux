#ifndef __BACKPORT_LINUX_UUID_H_
#define __BACKPORT_LINUX_UUID_H_
#include <linux/version.h>
#include <linux/uuid.h>

#ifndef UUID_STRING_LEN
/*
 * The length of a UUID string ("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
 * not including trailing NUL.
 */
#define	UUID_STRING_LEN		36
#endif

#define guid_t uuid_le
#define uuid_t uuid_be
#define UUID_INIT UUID_BE

static inline void guid_gen(guid_t *u)
{
	return uuid_le_gen(u);
}
static inline void uuid_gen(uuid_t *u)
{
	return uuid_be_gen(u);
}

static inline void guid_copy(guid_t *dst, const guid_t *src)
{
	memcpy(dst, src, sizeof(guid_t));
}

static inline int uuid_parse(const char *uuid, uuid_t *u)
{
	return uuid_be_to_bin(uuid, u);
}

static inline bool uuid_equal(const uuid_t *u1, const uuid_t *u2)
{
	return memcmp(u1, u2, sizeof(uuid_t)) == 0;
}

#endif /* __BACKPORT_LINUX_UUID_H_ */
