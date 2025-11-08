/**
 * ion_sunxi.h - Allwinner sunxi SoC ION memory allocator extensions
 *
 * Platform-specific extensions to Android ION for Allwinner sunxi SoCs
 * (F1C100s used in Trimui Smart). Provides cache management, physical
 * address queries, and DMA copy operations for efficient video buffer handling.
 *
 * Key features:
 * - Cache flush operations for coherency between CPU and hardware
 * - Physical address access for DMA setup
 * - Hardware-accelerated memory copy (DMA engine)
 * - Secure memory heap support
 *
 * These extensions are necessary because:
 * 1. Video hardware needs physical addresses for DMA
 * 2. CPU and hardware caches must be synchronized manually
 * 3. Large buffer copies benefit from DMA acceleration
 *
 * @note Requires Allwinner kernel patches (Linux 3.10 with sunxi support)
 * @warning Cache operations affect system-wide performance, use carefully
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

// Sunxi custom heap types start after standard ION heap types
#define ION_HEAP_TYPE_SUNXI_START (ION_HEAP_TYPE_CUSTOM + 1)
#define ION_HEAP_TYPE_SECURE	  (ION_HEAP_TYPE_SUNXI_START)  // Secure memory (DRM)

/**
 * sunxi_cache_range - CPU cache flush range specification
 *
 * Defines a virtual address range for cache operations. Used to synchronize
 * CPU cache with ION buffers before/after hardware access.
 *
 * @start: Start virtual address (inclusive)
 * @end: End virtual address (exclusive)
 */
typedef struct {
	long 	start;
	long 	end;
}sunxi_cache_range;

/**
 * sunxi_phys_data - Physical address query for ION buffer
 *
 * Used with ION_IOC_SUNXI_PHYS_ADDR to get physical address for DMA operations.
 *
 * @handle: ION buffer handle (input)
 * @phys_addr: Physical address of buffer (output)
 * @size: Size of buffer in bytes (output)
 */
typedef struct {
	void *handle;
	unsigned int phys_addr;
	unsigned int size;
}sunxi_phys_data;

/**
 * DMA_BUF_MAXCNT - Maximum number of buffers in a DMA copy group
 *
 * Hardware DMA engine can batch up to 8 copy operations.
 */
#define DMA_BUF_MAXCNT 	8

/**
 * dma_buf_item - Single DMA copy operation descriptor
 *
 * Describes one memory-to-memory copy operation for the hardware DMA engine.
 * Can use virtual or physical addresses.
 *
 * @src_va: Source virtual address (or 0 if using physical)
 * @src_pa: Source physical address (or 0 if using virtual)
 * @dst_va: Destination virtual address (or 0 if using physical)
 * @dst_pa: Destination physical address (or 0 if using virtual)
 * @size: Number of bytes to copy
 */
typedef struct {
	unsigned int src_va;
	unsigned int src_pa;
	unsigned int dst_va;
	unsigned int dst_pa;
	unsigned int size;
}dma_buf_item;

/**
 * dma_buf_group - Batch DMA copy operation group
 *
 * Allows submitting multiple copy operations to the DMA engine in one call
 * for improved efficiency.
 *
 * @multi_dma: True if multiple operations, false for single
 * @cnt: Number of valid items in array (1-8)
 * @item: Array of DMA copy descriptors
 */
typedef struct {
	bool multi_dma;
	unsigned int cnt;
	dma_buf_item item[DMA_BUF_MAXCNT];
}dma_buf_group;

///////////////////////////////
// Sunxi ION ioctl command numbers
///////////////////////////////

#define ION_IOC_SUNXI_FLUSH_RANGE           5   // Flush CPU cache for address range
#define ION_IOC_SUNXI_FLUSH_ALL             6   // Flush entire CPU cache
#define ION_IOC_SUNXI_PHYS_ADDR             7   // Get physical address from handle
#define ION_IOC_SUNXI_DMA_COPY              8   // Hardware-accelerated memory copy
#define ION_IOC_SUNXI_DUMP                  9   // Debug: dump ION state
#define ION_IOC_SUNXI_POOL_FREE             10  // Force free memory pool
///////////////////////////////
// Cache management functions
///////////////////////////////

/**
 * Flush and clean CPU cache for address range.
 *
 * Writes dirty cache lines back to memory and invalidates them.
 * Required before hardware reads memory written by CPU.
 *
 * @param start Start virtual address
 * @param end End virtual address (exclusive)
 * @return 0 on success, negative on error
 */
int flush_clean_user_range(long start, long end);

/**
 * Invalidate CPU cache for address range.
 *
 * Discards cached data, forcing CPU to re-read from memory.
 * Required after hardware writes memory that CPU will read.
 *
 * @param start Start virtual address
 * @param end End virtual address (exclusive)
 * @return 0 on success, negative on error
 */
int flush_user_range(long start, long end);

/**
 * Flush entire CPU data cache.
 *
 * @warning Very expensive operation, affects system-wide performance.
 *          Use range-based flushes when possible.
 */
void flush_dcache_all(void);

///////////////////////////////
// Sunxi memory allocation helpers
// These provide alternatives to ION for simple allocations
///////////////////////////////

/**
 * Allocates physically contiguous memory.
 *
 * Convenience wrapper around ION allocation for simple use cases.
 *
 * @param size Size in bytes to allocate
 * @param paddr Output parameter for physical address
 * @return Virtual address of allocated buffer, or NULL on failure
 *
 * @warning Caller must call sunxi_buf_free() to release memory
 */
void *sunxi_buf_alloc(unsigned int size, unsigned int *paddr);

/**
 * Frees buffer allocated by sunxi_buf_alloc().
 *
 * @param vaddr Virtual address returned by sunxi_buf_alloc()
 * @param paddr Physical address returned by sunxi_buf_alloc()
 * @param size Size in bytes (must match allocation size)
 */
void sunxi_buf_free(void *vaddr, unsigned int paddr, unsigned int size);

/**
 * Allocates physically contiguous memory (physical address only).
 *
 * Similar to sunxi_buf_alloc() but doesn't map to virtual address space.
 * Useful when only hardware DMA needs to access the buffer.
 *
 * @param size Size in bytes to allocate
 * @return Physical address of allocated buffer, or 0 on failure
 *
 * @warning Caller must call sunxi_free_phys() to release memory
 */
u32 sunxi_alloc_phys(size_t size);

/**
 * Frees physical memory allocated by sunxi_alloc_phys().
 *
 * @param paddr Physical address returned by sunxi_alloc_phys()
 * @param size Size in bytes (must match allocation size)
 */
void sunxi_free_phys(u32 paddr, size_t size);

/**
 * Maps physical memory to kernel virtual address space.
 *
 * Creates vmalloc-based mapping of physical memory for CPU access.
 *
 * @param paddr Physical address to map
 * @param size Size in bytes of region
 * @return Virtual address mapping, or NULL on failure
 *
 * @warning Caller must call sunxi_unmap_kernel() to release mapping
 */
void *sunxi_map_kernel(unsigned int paddr, unsigned int size);

/**
 * Unmaps kernel virtual mapping created by sunxi_map_kernel().
 *
 * @param vaddr Virtual address returned by sunxi_map_kernel()
 * @param paddr Physical address that was mapped
 * @param size Size in bytes (must match map size)
 */
void sunxi_unmap_kernel(void *vaddr, unsigned int paddr, unsigned int size);

#endif
