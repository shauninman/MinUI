/**
 * ion-owl.h - Actions OWL SoC ION memory allocator extensions
 *
 * Platform-specific extensions to the Android ION memory allocator for
 * Actions Semiconductor OWL (ATM7059) SoC, used in the RG35xx device.
 *
 * ION provides a unified interface for allocating physically contiguous
 * memory buffers that can be shared between hardware blocks (GPU, video
 * decoder, display engine) without copying. This header defines OWL-specific
 * heap IDs and ioctl commands.
 *
 * Key concepts:
 * - Heap: A memory pool with specific properties (cached, contiguous, etc.)
 * - Handle: Opaque reference to an allocated buffer
 * - Physical address: Required for DMA operations by hardware blocks
 *
 * @note Requires kernel ION driver support (Linux 3.x series)
 * @warning Physical addresses are only valid for physically contiguous heaps
 *
 * Copyright 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#if !defined(__KERNEL__)
#define __user
#endif

#ifndef _UAPI_LINUX_ION_OWL_H
#define _UAPI_LINUX_ION_OWL_H

#include <linux/types.h>

/**
 * struct owl_ion_phys_data - Physical address query for ION buffer
 *
 * Used with OWL_ION_GET_PHY ioctl to retrieve the physical address of
 * an ION buffer for hardware DMA operations.
 *
 * @handle: ION buffer handle (input)
 * @phys_addr: Physical address of buffer (output)
 * @size: Size of buffer in bytes (output)
 */
struct owl_ion_phys_data {
	ion_user_handle_t handle;
	unsigned long phys_addr;
	size_t size;
};

/**
 * OWL-specific ION ioctl commands
 *
 * These extend the standard ION ioctls with platform-specific functionality.
 */
enum {
	OWL_ION_GET_PHY = 0,  // Get physical address from ION handle
};

/**
 * enum ion_heap_ids - OWL platform ION heap identifiers
 *
 * Defines the available memory heaps on Actions OWL SoC. Heap IDs are used
 * as bit masks in allocation requests, allowing fallback to alternative heaps.
 *
 * Allocation order (if multiple bits set):
 * 1. ION_HEAP_ID_PMEM (0) - Physically contiguous, pre-reserved memory
 * 2. ION_HEAP_ID_FB (8) - Framebuffer memory region
 * 3. ION_HEAP_ID_SYSTEM (12) - System memory (vmalloc)
 *
 * IDs are spaced to allow insertion of new heap types without breaking compatibility.
 *
 * @note Don't change heap order without understanding the platform memory map
 */
enum ion_heap_ids {
	ION_HEAP_ID_INVALID = -1,  // Invalid heap ID
	ION_HEAP_ID_PMEM = 0,      // Pre-reserved physical memory (fastest, limited size)
	ION_HEAP_ID_FB = 8,        // Framebuffer memory region
	ION_HEAP_ID_SYSTEM = 12,   // System memory heap (largest, may not be contiguous)
	ION_HEAP_ID_RESERVED = 31  // Bit reserved for ION_SECURE flag
};

#endif /* _UAPI_LINUX_ION_OWL_H */
