/*
 * Copyright (C) 2005-2015 Junjiro R. Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * file and vm operations
 */

#include <linux/aio.h>
#include <linux/fs_stack.h>
#include <linux/mman.h>
#include <linux/security.h>
#include "aufs.h"

int au_do_open_nondir(struct file *file, int flags, struct file *h_file)
{
	int err;
	aufs_bindex_t bindex;
	struct dentry *dentry;
	struct au_finfo *finfo;
	struct inode *h_inode;

	FiMustWriteLock(file);

	err = 0;
	dentry = file->f_path.dentry;
	AuDebugOn(IS_ERR_OR_NULL(dentry));
	finfo = au_fi(file);
	memset(&finfo->fi_htop, 0, sizeof(finfo->fi_htop));
	atomic_set(&finfo->fi_mmapped, 0);
	bindex = au_dbstart(dentry);
	if (!h_file)
		h_file = au_h_open(dentry, bindex, flags, file, /*force_wr*/0);
	else
		get_file(h_file);
	if (IS_ERR(h_file))
		err = PTR_ERR(h_file);
	else {
		if ((flags & __O_TMPFILE)
		    && !(flags & O_EXCL)) {
			h_inode = file_inode(h_file);
			spin_lock(&h_inode->i_lock);
			h_inode->i_state |= I_LINKABLE;
			spin_unlock(&h_inode->i_lock);
		}
		au_set_fbstart(file, bindex);
		au_set_h_fptr(file, bindex, h_file);
		au_update_figen(file);
		/* todo: necessary? */
		/* file->f_ra = h_file->f_ra; */
	}

	return err;
}

static int aufs_open_nondir(struct inode *inode __maybe_unused,
			    struct file *file)
{
	int err;
	struct super_block *sb;
	struct au_do_open_args args = {
		.open	= au_do_open_nondir
	};

	AuDbg("%pD, f_flags 0x%x, f_mode 0x%x\n",
	      file, vfsub_file_flags(file), file->f_mode);

	sb = file->f_path.dentry->d_sb;
	si_read_lock(sb, AuLock_FLUSH);
	err = au_do_open(file, &args);
	si_read_unlock(sb);
	return err;
}

int aufs_release_nondir(struct inode *inode __maybe_unused, struct file *file)
{
	struct au_finfo *finfo;
	aufs_bindex_t bindex;

	finfo = au_fi(file);
	au_sphl_del(&finfo->fi_hlist,
		    &au_sbi(file->f_path.dentry->d_sb)->si_files);
	bindex = finfo->fi_btop;
	if (bindex >= 0)
		au_set_h_fptr(file, bindex, NULL);

	au_finfo_fin(file);
	return 0;
}

/* ---------------------------------------------------------------------- */

static int au_do_flush_nondir(struct file *file, fl_owner_t id)
{
	int err;
	struct file *h_file;

	err = 0;
	h_file = au_hf_top(file);
	if (h_file)
		err = vfsub_flush(h_file, id);
	return err;
}

static int aufs_flush_nondir(struct file *file, fl_owner_t id)
{
	return au_do_flush(file, id, au_do_flush_nondir);
}

/* ---------------------------------------------------------------------- */
/*
 * read and write functions acquire [fdi]_rwsem once, but release before
 * mmap_sem. This is because to stop a race condition between mmap(2).
 * Releasing these aufs-rwsem should be safe, no branch-mamagement (by keeping
 * si_rwsem), no harmful copy-up should happen. Actually copy-up may happen in
 * read functions after [fdi]_rwsem are released, but it should be harmless.
 */

/* Callers should call au_read_post() or fput() in the end */
struct file *au_read_pre(struct file *file, int keep_fi)
{
	struct file *h_file;
	int err;

	err = au_reval_and_lock_fdi(file, au_reopen_nondir, /*wlock*/0);
	if (!err) {
		di_read_unlock(file->f_path.dentry, AuLock_IR);
		h_file = au_hf_top(file);
		get_file(h_file);
		if (!keep_fi)
			fi_read_unlock(file);
	} else
		h_file = ERR_PTR(err);

	return h_file;
}

static void au_read_post(struct inode *inode, struct file *h_file)
{
	/* update without lock, I don't think it a problem */
	fsstack_copy_attr_atime(inode, file_inode(h_file));
	fput(h_file);
}

struct au_write_pre {
	blkcnt_t blks;
	aufs_bindex_t bstart;
};

/*
 * return with iinfo is write-locked
 * callers should call au_write_post() or iinfo_write_unlock() + fput() in the
 * end
 */
static struct file *au_write_pre(struct file *file, int do_ready,
				 struct au_write_pre *wpre)
{
	struct file *h_file;
	struct dentry *dentry;
	int err;
	struct au_pin pin;

	err = au_reval_and_lock_fdi(file, au_reopen_nondir, /*wlock*/1);
	h_file = ERR_PTR(err);
	if (unlikely(err))
		goto out;

	dentry = file->f_path.dentry;
	if (do_ready) {
		err = au_ready_to_write(file, -1, &pin);
		if (unlikely(err)) {
			h_file = ERR_PTR(err);
			di_write_unlock(dentry);
			goto out_fi;
		}
	}

	di_downgrade_lock(dentry, /*flags*/0);
	if (wpre)
		wpre->bstart = au_fbstart(file);
	h_file = au_hf_top(file);
	get_file(h_file);
	if (wpre)
		wpre->blks = file_inode(h_file)->i_blocks;
	if (do_ready)
		au_unpin(&pin);
	di_read_unlock(dentry, /*flags*/0);

out_fi:
	fi_write_unlock(file);
out:
	return h_file;
}

static void au_write_post(struct inode *inode, struct file *h_file,
			  struct au_write_pre *wpre, ssize_t written)
{
	struct inode *h_inode;

	au_cpup_attr_timesizes(inode);
	AuDebugOn(au_ibstart(inode) != wpre->bstart);
	h_inode = file_inode(h_file);
	inode->i_mode = h_inode->i_mode;
	ii_write_unlock(inode);
	fput(h_file);

	/* AuDbg("blks %llu, %llu\n", (u64)blks, (u64)h_inode->i_blocks); */
	if (written > 0)
		au_fhsm_wrote(inode->i_sb, wpre->bstart,
			      /*force*/h_inode->i_blocks > wpre->blks);
}

static ssize_t aufs_read(struct file *file, char __user *buf, size_t count,
			 loff_t *ppos)
{
	ssize_t err;
	struct inode *inode;
	struct file *h_file;
	struct super_block *sb;

	inode = file_inode(file);
	sb = inode->i_sb;
	si_read_lock(sb, AuLock_FLUSH | AuLock_NOPLMW);

	h_file = au_read_pre(file, /*keep_fi*/0);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	/* filedata may be obsoleted by concurrent copyup, but no problem */
	err = vfsub_read_u(h_file, buf, count, ppos);
	/* todo: necessary? */
	/* file->f_ra = h_file->f_ra; */
	au_read_post(inode, h_file);

out:
	si_read_unlock(sb);
	return err;
}

/*
 * todo: very ugly
 * it locks both of i_mutex and si_rwsem for read in safe.
 * if the plink maintenance mode continues forever (that is the problem),
 * may loop forever.
 */
static void au_mtx_and_read_lock(struct inode *inode)
{
	int err;
	struct super_block *sb = inode->i_sb;

	while (1) {
		mutex_lock(&inode->i_mutex);
		err = si_read_lock(sb, AuLock_FLUSH | AuLock_NOPLM);
		if (!err)
			break;
		mutex_unlock(&inode->i_mutex);
		si_read_lock(sb, AuLock_NOPLMW);
		si_read_unlock(sb);
	}
}

static ssize_t aufs_write(struct file *file, const char __user *ubuf,
			  size_t count, loff_t *ppos)
{
	ssize_t err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *h_file;
	char __user *buf = (char __user *)ubuf;

	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	err = vfsub_write_u(h_file, buf, count, ppos);
	au_write_post(inode, h_file, &wpre, err);

out:
	si_read_unlock(inode->i_sb);
	mutex_unlock(&inode->i_mutex);
	return err;
}

static ssize_t au_do_iter(struct file *h_file, int rw, struct kiocb *kio,
			  struct iov_iter *iov_iter)
{
	ssize_t err;
	struct file *file;
	ssize_t (*iter)(struct kiocb *, struct iov_iter *);

	err = security_file_permission(h_file, rw);
	if (unlikely(err))
		goto out;

	err = -ENOSYS;
	iter = NULL;
	if (rw == MAY_READ)
		iter = h_file->f_op->read_iter;
	else if (rw == MAY_WRITE)
		iter = h_file->f_op->write_iter;

	file = kio->ki_filp;
	kio->ki_filp = h_file;
	if (iter) {
		lockdep_off();
		err = iter(kio, iov_iter);
		lockdep_on();
	} else
		/* currently there is no such fs */
		WARN_ON_ONCE(1);
	kio->ki_filp = file;

out:
	return err;
}

static ssize_t aufs_read_iter(struct kiocb *kio, struct iov_iter *iov_iter)
{
	ssize_t err;
	struct file *file, *h_file;
	struct inode *inode;
	struct super_block *sb;

	file = kio->ki_filp;
	inode = file_inode(file);
	sb = inode->i_sb;
	si_read_lock(sb, AuLock_FLUSH | AuLock_NOPLMW);

	h_file = au_read_pre(file, /*keep_fi*/0);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	err = au_do_iter(h_file, MAY_READ, kio, iov_iter);
	/* todo: necessary? */
	/* file->f_ra = h_file->f_ra; */
	au_read_post(inode, h_file);

out:
	si_read_unlock(sb);
	return err;
}

static ssize_t aufs_write_iter(struct kiocb *kio, struct iov_iter *iov_iter)
{
	ssize_t err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *file, *h_file;

	file = kio->ki_filp;
	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	err = au_do_iter(h_file, MAY_WRITE, kio, iov_iter);
	au_write_post(inode, h_file, &wpre, err);

out:
	si_read_unlock(inode->i_sb);
	mutex_unlock(&inode->i_mutex);
	return err;
}

static ssize_t aufs_splice_read(struct file *file, loff_t *ppos,
				struct pipe_inode_info *pipe, size_t len,
				unsigned int flags)
{
	ssize_t err;
	struct file *h_file;
	struct inode *inode;
	struct super_block *sb;

	inode = file_inode(file);
	sb = inode->i_sb;
	si_read_lock(sb, AuLock_FLUSH | AuLock_NOPLMW);

	h_file = au_read_pre(file, /*keep_fi*/1);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	if (au_test_loopback_kthread()) {
		au_warn_loopback(h_file->f_path.dentry->d_sb);
		if (file->f_mapping != h_file->f_mapping) {
			file->f_mapping = h_file->f_mapping;
			smp_mb(); /* unnecessary? */
		}
	}
	fi_read_unlock(file);

	err = vfsub_splice_to(h_file, ppos, pipe, len, flags);
	/* todo: necessasry? */
	/* file->f_ra = h_file->f_ra; */
	au_read_post(inode, h_file);

out:
	si_read_unlock(sb);
	return err;
}

static ssize_t
aufs_splice_write(struct pipe_inode_info *pipe, struct file *file, loff_t *ppos,
		  size_t len, unsigned int flags)
{
	ssize_t err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *h_file;

	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	err = vfsub_splice_from(pipe, h_file, ppos, len, flags);
	au_write_post(inode, h_file, &wpre, err);

out:
	si_read_unlock(inode->i_sb);
	mutex_unlock(&inode->i_mutex);
	return err;
}

static long aufs_fallocate(struct file *file, int mode, loff_t offset,
			   loff_t len)
{
	long err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *h_file;

	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	lockdep_off();
	err = vfs_fallocate(h_file, mode, offset, len);
	lockdep_on();
	au_write_post(inode, h_file, &wpre, /*written*/1);

out:
	si_read_unlock(inode->i_sb);
	mutex_unlock(&inode->i_mutex);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * The locking order around current->mmap_sem.
 * - in most and regular cases
 *   file I/O syscall -- aufs_read() or something
 *	-- si_rwsem for read -- mmap_sem
 *	(Note that [fdi]i_rwsem are released before mmap_sem).
 * - in mmap case
 *   mmap(2) -- mmap_sem -- aufs_mmap() -- si_rwsem for read -- [fdi]i_rwsem
 * This AB-BA order is definitly bad, but is not a problem since "si_rwsem for
 * read" allows muliple processes to acquire it and [fdi]i_rwsem are not held in
 * file I/O. Aufs needs to stop lockdep in aufs_mmap() though.
 * It means that when aufs acquires si_rwsem for write, the process should never
 * acquire mmap_sem.
 *
 * Actually aufs_iterate() holds [fdi]i_rwsem before mmap_sem, but this is not a
 * problem either since any directory is not able to be mmap-ed.
 * The similar scenario is applied to aufs_readlink() too.
 */

#if 0 /* stop calling security_file_mmap() */
/* cf. linux/include/linux/mman.h: calc_vm_prot_bits() */
#define AuConv_VM_PROT(f, b)	_calc_vm_trans(f, VM_##b, PROT_##b)

static unsigned long au_arch_prot_conv(unsigned long flags)
{
	/* currently ppc64 only */
#ifdef CONFIG_PPC64
	/* cf. linux/arch/powerpc/include/asm/mman.h */
	AuDebugOn(arch_calc_vm_prot_bits(-1) != VM_SAO);
	return AuConv_VM_PROT(flags, SAO);
#else
	AuDebugOn(arch_calc_vm_prot_bits(-1));
	return 0;
#endif
}

static unsigned long au_prot_conv(unsigned long flags)
{
	return AuConv_VM_PROT(flags, READ)
		| AuConv_VM_PROT(flags, WRITE)
		| AuConv_VM_PROT(flags, EXEC)
		| au_arch_prot_conv(flags);
}

/* cf. linux/include/linux/mman.h: calc_vm_flag_bits() */
#define AuConv_VM_MAP(f, b)	_calc_vm_trans(f, VM_##b, MAP_##b)

static unsigned long au_flag_conv(unsigned long flags)
{
	return AuConv_VM_MAP(flags, GROWSDOWN)
		| AuConv_VM_MAP(flags, DENYWRITE)
		| AuConv_VM_MAP(flags, LOCKED);
}
#endif

static int aufs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err;
	const unsigned char wlock
		= (file->f_mode & FMODE_WRITE) && (vma->vm_flags & VM_SHARED);
	struct super_block *sb;
	struct file *h_file;
	struct inode *inode;

	AuDbgVmRegion(file, vma);

	inode = file_inode(file);
	sb = inode->i_sb;
	lockdep_off();
	si_read_lock(sb, AuLock_NOPLMW);

	h_file = au_write_pre(file, wlock, /*wpre*/NULL);
	lockdep_on();
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	err = 0;
	au_set_mmapped(file);
	au_vm_file_reset(vma, h_file);
	/*
	 * we cannot call security_mmap_file() here since it may acquire
	 * mmap_sem or i_mutex.
	 *
	 * err = security_mmap_file(h_file, au_prot_conv(vma->vm_flags),
	 *			 au_flag_conv(vma->vm_flags));
	 */
	if (!err)
		err = h_file->f_op->mmap(h_file, vma);
	if (!err) {
		au_vm_prfile_set(vma, file);
		fsstack_copy_attr_atime(inode, file_inode(h_file));
		goto out_fput; /* success */
	}
	au_unset_mmapped(file);
	au_vm_file_reset(vma, file);

out_fput:
	lockdep_off();
	ii_write_unlock(inode);
	lockdep_on();
	fput(h_file);
out:
	lockdep_off();
	si_read_unlock(sb);
	lockdep_on();
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int aufs_fsync_nondir(struct file *file, loff_t start, loff_t end,
			     int datasync)
{
	int err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *h_file;

	err = 0; /* -EBADF; */ /* posix? */
	if (unlikely(!(file->f_mode & FMODE_WRITE)))
		goto out;

	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out_unlock;

	err = vfsub_fsync(h_file, &h_file->f_path, datasync);
	au_write_post(inode, h_file, &wpre, /*written*/0);

out_unlock:
	si_read_unlock(inode->i_sb);
	mutex_unlock(&inode->i_mutex);
out:
	return err;
}

/* no one supports this operation, currently */
#if 0
static int aufs_aio_fsync_nondir(struct kiocb *kio, int datasync)
{
	int err;
	struct au_write_pre wpre;
	struct inode *inode;
	struct file *file, *h_file;

	err = 0; /* -EBADF; */ /* posix? */
	if (unlikely(!(file->f_mode & FMODE_WRITE)))
		goto out;

	file = kio->ki_filp;
	inode = file_inode(file);
	au_mtx_and_read_lock(inode);

	h_file = au_write_pre(file, /*do_ready*/1, &wpre);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out_unlock;

	err = -ENOSYS;
	h_file = au_hf_top(file);
	if (h_file->f_op->aio_fsync) {
		struct mutex *h_mtx;

		h_mtx = &file_inode(h_file)->i_mutex;
		if (!is_sync_kiocb(kio)) {
			get_file(h_file);
			fput(file);
		}
		kio->ki_filp = h_file;
		err = h_file->f_op->aio_fsync(kio, datasync);
		mutex_lock_nested(h_mtx, AuLsc_I_CHILD);
		if (!err)
			vfsub_update_h_iattr(&h_file->f_path, /*did*/NULL);
		/*ignore*/
		mutex_unlock(h_mtx);
	}
	au_write_post(inode, h_file, &wpre, /*written*/0);

out_unlock:
	si_read_unlock(inode->sb);
	mutex_unlock(&inode->i_mutex);
out:
	return err;
}
#endif

static int aufs_fasync(int fd, struct file *file, int flag)
{
	int err;
	struct file *h_file;
	struct super_block *sb;

	sb = file->f_path.dentry->d_sb;
	si_read_lock(sb, AuLock_FLUSH | AuLock_NOPLMW);

	h_file = au_read_pre(file, /*keep_fi*/0);
	err = PTR_ERR(h_file);
	if (IS_ERR(h_file))
		goto out;

	if (h_file->f_op->fasync)
		err = h_file->f_op->fasync(fd, h_file, flag);
	fput(h_file); /* instead of au_read_post() */

out:
	si_read_unlock(sb);
	return err;
}

/* ---------------------------------------------------------------------- */

/* no one supports this operation, currently */
#if 0
static ssize_t aufs_sendpage(struct file *file, struct page *page, int offset,
			     size_t len, loff_t *pos, int more)
{
}
#endif

/* ---------------------------------------------------------------------- */

const struct file_operations aufs_file_fop = {
	.owner		= THIS_MODULE,

	.llseek		= default_llseek,

	.read		= aufs_read,
	.write		= aufs_write,
	.read_iter	= aufs_read_iter,
	.write_iter	= aufs_write_iter,

#ifdef CONFIG_AUFS_POLL
	.poll		= aufs_poll,
#endif
	.unlocked_ioctl	= aufs_ioctl_nondir,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= aufs_compat_ioctl_nondir,
#endif
	.mmap		= aufs_mmap,
	.open		= aufs_open_nondir,
	.flush		= aufs_flush_nondir,
	.release	= aufs_release_nondir,
	.fsync		= aufs_fsync_nondir,
	/* .aio_fsync	= aufs_aio_fsync_nondir, */
	.fasync		= aufs_fasync,
	/* .sendpage	= aufs_sendpage, */
	.splice_write	= aufs_splice_write,
	.splice_read	= aufs_splice_read,
#if 0
	.aio_splice_write = aufs_aio_splice_write,
	.aio_splice_read  = aufs_aio_splice_read,
#endif
	.fallocate	= aufs_fallocate
};
