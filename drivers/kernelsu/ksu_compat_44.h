/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ksu_compat_44.h — single header with all kernel 4.4 compatibility shims.
 * #include'd by the Makefile via -include, so no per-file change is needed.
 */
#ifndef _KSU_COMPAT_44_H
#define _KSU_COMPAT_44_H

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/security.h>

/* ── untagged_addr: MTE/TBI pointer tagging — absent in 4.4 ─────────────── */
#ifndef untagged_addr
# define untagged_addr(addr) (addr)
#endif

/* ── mmap lock: mmap_sem renamed to mmap_lock API in 5.8 ────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
# define mmap_read_lock(mm)       down_read(&(mm)->mmap_sem)
# define mmap_read_unlock(mm)     up_read(&(mm)->mmap_sem)
# define mmap_write_lock(mm)      down_write(&(mm)->mmap_sem)
# define mmap_write_unlock(mm)    up_write(&(mm)->mmap_sem)
# define mmap_read_trylock(mm)    down_read_trylock(&(mm)->mmap_sem)
#endif

/* ── task_work_add: TWA_RESUME flag introduced in 4.20 ──────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0)
# ifdef TWA_RESUME
#  undef TWA_RESUME
# endif
# define TWA_RESUME true
/* In 4.4, task_work_add(task, work, notify) takes bool not enum */
#endif

/* ── strncpy_from_user_nofault: introduced in 5.8 ───────────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
# define strncpy_from_user_nofault(dst, src, size) \
    strncpy_from_user((dst), (src), (size))
# define copy_from_user_nofault(dst, src, size) \
    ({ int __ret = -EFAULT; \
       if (access_ok(VERIFY_READ, (src), (size))) \
           __ret = __copy_from_user_inatomic((dst), (src), (size)); \
       __ret ? -EFAULT : 0; })
#endif

/* ── ksys_close/ksys_read/ksys_write: added in 4.17 ─────────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
# include <linux/syscalls.h>
  static inline long ksu_close_fd(unsigned int fd)
  {
      /* Use filp_close on the looked-up file */
      struct file *f = fget(fd);
      if (!f)
          return -EBADF;
      fput(f);
      return sys_close(fd);
  }
# define ksys_close(fd)           ksu_close_fd(fd)
# define ksys_unshare(flags)      sys_unshare(flags)
#endif

/* ── kernel_write: API changed in 4.14 (loff_t* vs loff_t) ─────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static inline ssize_t ksu_kernel_write(struct file *file, const void *buf,
                                       size_t count, loff_t *pos)
{
    mm_segment_t old_fs = get_fs();
    ssize_t ret;
    set_fs(KERNEL_DS);
    ret = vfs_write(file, (const char __force __user *)buf, count, pos);
    set_fs(old_fs);
    return ret;
}
# define kernel_write(file, buf, count, ppos) \
    ksu_kernel_write((file), (buf), (count), (ppos))
#endif

/* ── kernel_read: API changed in 4.14 ───────────────────────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static inline ssize_t ksu_kernel_read(struct file *file, void *buf,
                                      size_t count, loff_t *pos)
{
    mm_segment_t old_fs = get_fs();
    ssize_t ret;
    set_fs(KERNEL_DS);
    ret = vfs_read(file, (char __force __user *)buf, count, pos);
    set_fs(old_fs);
    return ret;
}
# define kernel_read(file, buf, count, ppos) \
    ksu_kernel_read((file), (buf), (count), (ppos))
#endif

/* ── full_name_hash: gained a 'salt' arg in 4.8 ─────────────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
# define ksu_full_name_hash(salt, name, len) full_name_hash((name), (len))
#else
# define ksu_full_name_hash(salt, name, len) full_name_hash((salt), (name), (len))
#endif

/* ── seccomp filter_count: field absent in 4.4 ───────────────────────────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
# ifdef CONFIG_SECCOMP_FILTER
static inline int ksu_seccomp_filter_count(struct task_struct *t)
{
   return t->seccomp.filter_count;
}
# else
#  define ksu_seccomp_filter_count(t) 0
# endif
/* Patch app_profile.c to call ksu_seccomp_filter_count(t) instead of
   task->seccomp.filter_count */
#endif

/* ── group_info gid accessor ─────────────────────────────────────────────── */
/* In 4.4+, kgid_t is a struct { gid_t val; } under CONFIG_UIDGID_STRICT_TYPE_CHECKS */
#ifndef KSU_GROUP_GID
# if defined(CONFIG_UIDGID_STRICT_TYPE_CHECKS)
#  define KSU_GROUP_GID(gi, i) ((gi)->gid[(i)].val)
# else
#  define KSU_GROUP_GID(gi, i) ((gi)->gid[(i)])
# endif
#endif

/* ── selinux_state: struct added in ~4.17; 4.4 uses global variables ──────── */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
# define ksu_selinux_enforcing()       (selinux_enforcing)
# define ksu_set_selinux_enforcing(v)  do { selinux_enforcing = (v); } while (0)
#else
# include <linux/lsm_hooks.h>
# define ksu_selinux_enforcing()       (selinux_state.enforcing)
# define ksu_set_selinux_enforcing(v)  do { selinux_state.enforcing = (v); } while (0)
#endif

#endif /* _KSU_COMPAT_44_H */
