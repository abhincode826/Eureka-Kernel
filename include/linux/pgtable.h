/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KernelSU-Next compat shim: linux/pgtable.h introduced post-4.4.
 * In 4.4 the page table code is in asm/pgtable.h.
 *
 * Also provides p4d_t stubs for 5-level paging code that unconditionally
 * references p4d — 4.4 uses 4-level paging (pgd→pud→pmd→pte).
 */
#ifndef _LINUX_PGTABLE_COMPAT_H
#define _LINUX_PGTABLE_COMPAT_H

#include <asm/pgtable.h>
#include <asm-generic/pgtable.h>

/* ------------ p4d stubs (5-level paging absent in 4.4) ------------ */
#ifndef __PAGETABLE_P4D_FOLDED
# define __PAGETABLE_P4D_FOLDED 1
#endif

typedef struct { pgd_t pgd; } p4d_t;

static inline p4d_t *p4d_offset(pgd_t *pgd, unsigned long addr)
{
	return (p4d_t *)pgd;
}
static inline int p4d_none(p4d_t p4d)  { return pgd_none(p4d.pgd); }
static inline int p4d_bad(p4d_t p4d)   { return pgd_bad(p4d.pgd); }
static inline int p4d_present(p4d_t p4d){ return pgd_present(p4d.pgd); }
static inline unsigned long p4d_pfn(p4d_t p4d) { return pgd_pfn(p4d.pgd); }
static inline pud_t *p4d_pgtable(p4d_t p4d)
{
	return (pud_t *)pgd_page_vaddr(p4d.pgd);
}

#endif /* _LINUX_PGTABLE_COMPAT_H */
