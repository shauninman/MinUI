/*
 * include/linux/ion_sunxi.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *
 * sunxi ion header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ION_SUNXI_H
#define __ION_SUNXI_H

#define ION_HEAP_TYPE_SUNXI_START (ION_HEAP_TYPE_CUSTOM + 1)
#define ION_HEAP_TYPE_SECURE	  (ION_HEAP_TYPE_SUNXI_START)

typedef struct {
	long 	start;
	long 	end;
}sunxi_cache_range;

typedef struct {
	void *handle;
	unsigned int phys_addr;
	unsigned int size;
}sunxi_phys_data;


#define DMA_BUF_MAXCNT 	8

typedef struct {
	unsigned int src_va;
	unsigned int src_pa;
	unsigned int dst_va;
	unsigned int dst_pa;
	unsigned int size;
}dma_buf_item;

typedef struct {
	bool multi_dma;
	unsigned int cnt;
	dma_buf_item item[DMA_BUF_MAXCNT];
}dma_buf_group;

#define ION_IOC_SUNXI_FLUSH_RANGE           5
#define ION_IOC_SUNXI_FLUSH_ALL             6
#define ION_IOC_SUNXI_PHYS_ADDR             7
#define ION_IOC_SUNXI_DMA_COPY              8
#define ION_IOC_SUNXI_DUMP                  9
#define ION_IOC_SUNXI_POOL_FREE             10
int flush_clean_user_range(long start, long end);
int flush_user_range(long start, long end);
void flush_dcache_all(void);

/**
 * sunxi_buf_alloc - alloc phys contigous memory in SUNXI platform.
 * @size: size in bytes to allocate.
 * @paddr: store the start phys address allocated.
 *
 * return the start virtual address, or 0 if failed.
 */
void *sunxi_buf_alloc(unsigned int size, unsigned int *paddr);
/**
 * sunxi_buf_free - free buffer allocated by sunxi_buf_alloc.
 * @vaddr: the kernel virt addr of the area.
 * @paddr: the start phys addr of the area.
 * @size: size in bytes of the area.
 */
void sunxi_buf_free(void *vaddr, unsigned int paddr, unsigned int size);
/**
 * sunxi_alloc_phys - alloc phys contigous memory in SUNXI platform.
 * @size: size in bytes to allocate.
 *
 * return the start phys addr, or 0 if failed.
 */
u32 sunxi_alloc_phys(size_t size);
/**
 * sunxi_free_phys - free phys contigous memory allocted by sunxi_alloc_phys.
 * @paddr: the start phys addr of the area.
 * @size: size in bytes of the area.
 */
void sunxi_free_phys(u32 paddr, size_t size);
/**
 * sunxi_map_kernel - map phys contigous memory to kernel virtual space.
 * @paddr: the start phys addr of the area.
 * @size: size in bytes of the area.
 *
 * return the start virt addr which is in vmalloc space, or NULL if failed.
 */
void *sunxi_map_kernel(unsigned int paddr, unsigned int size);
/**
 * sunxi_unmap_kernel - unmap phys contigous memory from kernel space.
 * @vaddr: the kernel virt addr of the area.
 * @paddr: the start phys addr of the area.
 * @size: size in bytes of the area.
 */
void sunxi_unmap_kernel(void *vaddr, unsigned int paddr, unsigned int size);

#endif
