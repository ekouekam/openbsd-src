/*	$OpenBSD: compat_linux.h,v 1.2 2005/01/10 21:10:58 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if 0

/* Define these unconditionally to get the .modinfo section. */
#undef __KERNEL__
#undef MODULE
#define __KERNEL__
#define MODULE

/* Standard headers for Linux LKMs */
#include <linux/kernel.h>
#include <linux/modsetver.h>
#include <linux/module.h>

/*
 * Include Linux 2.4.x headers.
 */
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <asm/mach/map.h>

#else

/*
 * Declare the things that we need from the Linux headers.
 */

#define	IS_ERR(ptr) ((unsigned long)(ptr) > (unsigned long)-1000L)

struct file;
struct inode;

typedef long loff_t;
typedef int ssize_t;
typedef unsigned int size_t;

struct file_operations {
	struct module *owner;
	void (*llseek) (void);
	ssize_t (*read) (struct file *, char *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char *, size_t, loff_t *);
	void (*readdir) (void);
	void (*poll) (void);
	void (*ioctl) (void);
	void (*mmap) (void);
	int (*open) (struct inode *, struct file *);
	void (*flush) (void);
	int (*release) (struct inode *, struct file *);
	void (*fsync) (void);
	void (*fasync) (void);
	void (*lock) (void);
	void (*readv) (void);
	void (*writev) (void);
	void (*sendpage) (void);
	void (*get_unmapped_area)(void);
#ifdef MAGIC_ROM_PTR
	void (*romptr) (void);
#endif /* MAGIC_ROM_PTR */
};

extern	struct file * open_exec(const char *);
extern	void fput(struct file *);
extern	int kernel_read(struct file *, unsigned long, char *, unsigned long);
extern	int memcmp(const void *, const void *, unsigned int);
extern	int register_chrdev(unsigned int, const char *, struct file_operations *);
extern	int unregister_chrdev(unsigned int, const char *);
extern	void printk(const char *, ...);
extern	void *memcpy(void *, const void *, size_t);

/* BSD headers */
#include <sys/exec_elf.h>
#include <errno.h>

/* Linux LKM support */
static const char __module_kernel_version[] __attribute__((section(".modinfo"))) =
"kernel_version=" UTS_RELEASE;
#if 1 /* def MODVERSIONS */
static const char __module_using_checksums[] __attribute__((section(".modinfo"))) =
"using_checksums=1";
#endif

#endif
