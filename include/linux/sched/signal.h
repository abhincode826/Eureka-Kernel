/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KernelSU-Next compat shim: linux/sched/signal.h does not exist in 4.4.
 * In 4.4 all signal/task functions are in linux/sched.h.
 */
#ifndef _LINUX_SCHED_SIGNAL_COMPAT_H
#define _LINUX_SCHED_SIGNAL_COMPAT_H
#include <linux/sched.h>
#endif /* _LINUX_SCHED_SIGNAL_COMPAT_H */
