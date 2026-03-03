// SPDX-License-Identifier: GPL-2.0
/*
 * pkg_observer.c — KernelSU-Next package observer (kernel 4.4, inotify-based)
 *
 * Watches /data/system/packages.list via fsnotify so the kernel is notified
 * immediately when Android's package manager updates it (install/uninstall/
 * update). On change, we signal ksud to refresh its UID allow-list.
 *
 * Uses only standard kernel 4.4 APIs — no extra patches needed.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/fsnotify_backend.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include "ksu.h"

#define PKG_LIST_PATH   "/data/system/packages.list"
#define OBSERVER_DELAY  (HZ / 2)   /* debounce: wait 500ms before refresh */

/* ── fsnotify group ─────────────────────────────────────────────────────── */
static struct fsnotify_group *ksu_pkg_group;

/* ── debounce workqueue ─────────────────────────────────────────────────── */
static struct delayed_work ksu_pkg_work;

static void ksu_pkg_refresh_work(struct work_struct *work)
{
	pr_info("kernelsu: pkg_observer: packages.list changed, refreshing UIDs\n");
	ksu_refresh_allow_list();
}

/* ── fsnotify event handler ─────────────────────────────────────────────── */
static int ksu_pkg_handle_event(struct fsnotify_group *group,
				struct inode *inode,
				struct fsnotify_mark *inode_mark,
				struct fsnotify_mark *vfsmount_mark,
				u32 mask,
				void *data,
				int data_type,
				const unsigned char *file_name,
				u32 cookie)
{
	/* Only care about close-write and moved-to events —
	 * package manager writes packages.list atomically via rename */
	if (!(mask & (FS_CLOSE_WRITE | FS_MOVED_TO | FS_MODIFY)))
		return 0;

	/* Debounce: cancel any pending work and reschedule */
	mod_delayed_work(system_wq, &ksu_pkg_work, OBSERVER_DELAY);
	return 0;
}

static void ksu_pkg_free_group_priv(struct fsnotify_group *group) {}
static void ksu_pkg_free_event(struct fsnotify_event *event) {}

static const struct fsnotify_ops ksu_pkg_fsnotify_ops = {
	.handle_event   = ksu_pkg_handle_event,
	.free_group_priv = ksu_pkg_free_group_priv,
	.free_event     = ksu_pkg_free_event,
};

/* ── mark setup ─────────────────────────────────────────────────────────── */
static struct fsnotify_mark ksu_pkg_mark;

static int ksu_pkg_add_watch(void)
{
	struct path path;
	struct inode *inode;
	int ret;

	ret = kern_path(PKG_LIST_PATH, LOOKUP_FOLLOW, &path);
	if (ret) {
		/* /data may not be mounted yet at early boot — that's fine,
		 * the watch will be installed on first ksu_observer_init()
		 * call after /data is available. */
		pr_info("kernelsu: pkg_observer: %s not found yet (ret=%d)\n",
			PKG_LIST_PATH, ret);
		return ret;
	}

	inode = d_inode(path.dentry);

	fsnotify_init_mark(&ksu_pkg_mark, ksu_pkg_group);
	ksu_pkg_mark.mask = FS_CLOSE_WRITE | FS_MOVED_TO | FS_MODIFY;

	ret = fsnotify_add_mark(&ksu_pkg_mark, ksu_pkg_group,
				inode, NULL, 0);
	if (ret)
		pr_warn("kernelsu: pkg_observer: fsnotify_add_mark failed: %d\n",
			ret);
	else
		pr_info("kernelsu: pkg_observer: watching %s\n", PKG_LIST_PATH);

	path_put(&path);
	return ret;
}

/* ── public API ─────────────────────────────────────────────────────────── */

int ksu_observer_init(void)
{
	int ret;

	INIT_DELAYED_WORK(&ksu_pkg_work, ksu_pkg_refresh_work);

	ksu_pkg_group = fsnotify_alloc_group(&ksu_pkg_fsnotify_ops);
	if (IS_ERR(ksu_pkg_group)) {
		ret = PTR_ERR(ksu_pkg_group);
		pr_err("kernelsu: pkg_observer: fsnotify_alloc_group failed: %d\n",
		       ret);
		ksu_pkg_group = NULL;
		return ret;
	}

	ret = ksu_pkg_add_watch();
	if (ret) {
		/* Non-fatal: root still works, just no live refresh */
		pr_warn("kernelsu: pkg_observer: running without live watch\n");
	}

	return 0;
}
EXPORT_SYMBOL(ksu_observer_init);

void ksu_observer_exit(void)
{
	cancel_delayed_work_sync(&ksu_pkg_work);

	if (ksu_pkg_group) {
		fsnotify_destroy_mark(&ksu_pkg_mark, ksu_pkg_group);
		fsnotify_put_group(ksu_pkg_group);
		ksu_pkg_group = NULL;
	}

	pr_info("kernelsu: pkg_observer: exit\n");
}
EXPORT_SYMBOL(ksu_observer_exit);
