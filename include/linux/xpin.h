/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_XPIN_H
#define _LINUX_XPIN_H


/* Set if an exception of some kind is granted for this file or task. */
#define XPIN_EXCEPTION_ALLOW             (1 << 0)

/*
 * No exception actually granted. Meant to be used in combination with "quiet"
 * to prevent logging audit messages for a task which still works normally even
 * when XPin denies some of its actions.
 */
#define XPIN_EXCEPTION_DENY              (1 << 1)

/*
 * Set if the exception should ONLY allow JIT. This flag allows mmap & mprotect
 * on anonymous memory, but not running unsigned programs.
 */
#define XPIN_EXCEPTION_JIT_ONLY          (1 << 2)

/* Exception is inherited across execve and forks */
#define XPIN_EXCEPTION_INHERIT           (1 << 3)

/* Set if the exception should allow code from non-ELF files */
#define XPIN_EXCEPTION_NON_ELF           (1 << 4)

/* Violations are not logged */
#define XPIN_EXCEPTION_QUIET             (1 << 5)

/* Set when AT_SECURE should be enabled for execs of this program */
#define XPIN_EXCEPTION_SECURE            (1 << 6)

/* Exception will override any inherited exception flags */
#define XPIN_EXCEPTION_UNINHERIT         (1 << 7)

#define XPIN_FILE_EXCEPTION_MASK ( \
	XPIN_EXCEPTION_ALLOW \
	| XPIN_EXCEPTION_DENY \
	| XPIN_EXCEPTION_JIT_ONLY \
	| XPIN_EXCEPTION_INHERIT \
	| XPIN_EXCEPTION_NON_ELF \
	| XPIN_EXCEPTION_QUIET \
	| XPIN_EXCEPTION_SECURE \
	| XPIN_EXCEPTION_UNINHERIT \
	)

/* Set after the inode is checked to be an ELF file or not */
#define XPIN_INODE_FORMAT_CHECKED        (1 << 0)

/*
 * Set if the inode is an ELF file. Not valid unless XPIN_INODE_FORMAT_CHECKED
 * is set.
 */
#define XPIN_INODE_IS_ELF                (1 << 1)

#define XPIN_INODE_MASK ( \
	XPIN_INODE_FORMAT_CHECKED \
	| XPIN_INODE_IS_ELF \
	)

/* Set after a superblock is checked as being a dm-verity volume or not */
#define XPIN_SB_CHECKED                  (1 << 0)

/*
 * Set if this is a dm-verity superblock. Not valid unless XPIN_SB_CHECKED is
 * set.
 */
#define XPIN_SB_VERITY                   (1 << 1)

/*
 * Set after the xpin_lazy_exception_list has been checked for entries which
 * apply to this superblock.
 */
#define XPIN_SB_LAZY_EXCEPTIONS_CHECKED  (1 << 2)

#define XPIN_SB_MASK ( \
	XPIN_SB_CHECKED \
	| XPIN_SB_VERITY \
	| XPIN_SB_LAZY_EXCEPTIONS_CHECKED \
	)


enum xpin_event {
	XPIN_EVENT_MMAP,
	XPIN_EVENT_MPROTECT,
	XPIN_EVENT_EXEC,
	XPIN_EVENT_EXEC_EXCEPTION,
	XPIN_EVENT_SHMAT,
	XPIN_EVENT_PROCMEM,
	XPIN_EVENT_CHECK,

	XPIN_EVENT_COUNT
};


/* Get a file's exception flags */
int xpin_file_get_exception_flags(struct file *file);

/* Get a task's exception flags */
int xpin_task_get_exception_flags(const struct task_struct *task);

/* Check if the superblock is a dm-verity volume with enforcement */
bool xpin_is_verity_sb(struct super_block *sb);

/* Check if the file is on a dm-verity volume */
bool xpin_is_verity_file(struct file *file);

/* Check if the file is an ELF file */
bool xpin_is_elf_file(struct file *file);

/* Check if the file was created by `memfd_create()` */
bool xpin_is_memfd_file(struct file *file);

/* Check if the file is a SYSV SHM file (created by `shmget()`) */
bool xpin_is_sysv_shm_file(struct file *file);

/* Common access checking routine for executable code operations */
int xpin_access_check(struct file *file, enum xpin_event event, bool mutable);

#endif /* _LINUX_XPIN_H */
