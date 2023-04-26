/*
 * linux/drivers/video/owl/dss/de_atm7059.h
 *
 * NOTE: SHOULD only be included by de_atm7059.c
 *
 * Copyright (C) 2014 Actions
 * Author: lipeng<lipeng@actions-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DE_ATM7059_H_
#define _DE_ATM7059_H_

//#include "de.h"

/*================================================================
 *           Definition of registers and bit position
 *==============================================================*/


/* please fixme */
#define DE_SIZE_BIT_WIDTH			12

/*
 * DE common registers
 */
#define DE_IRQSTATUS				0x0004
#define DE_IRQENABLE				0x0000
#define DE_IF_CON				0x000c

#define DE_MMU_EN				0x0010
#define DE_MMU_BASE				0x0014

#define DE_OUTPUT_CON				0x1000
#define DE_OUTPUT_STAT				0x100c
#define DE_WB_CON				0x1004
#define DE_WB_ADDR				0x1008

/* dither for path0, only for ATM7059A */
#define DE_PATH_DITHER				0x150

/*
 * dehw manager/channel specific registers
 */
#define DE_PATH_BASE				0x0100
#define DE_PATH_CTL(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0000)

#define DE_PATH_EN(n)				DE_PATH_CTL(n)
#define DE_PATH_ENABLE_BIT			28

#define DE_PATH_FCR(n)				DE_PATH_CTL(n)
#define DE_PATH_BK(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0020)
#define DE_PATH_SIZE(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0024)
#define DE_PATH_E_COOR(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0028)

#define DE_PATH_GAMMA_IDX(n)			(DE_PATH_BASE + (n) * 0x100  +  0x002C)
#define DE_PATH_GAMMA_IDX_BUSY_BIT		(14)
#define DE_PATH_GAMMA_IDX_OP_SEL_BEGIN_BIT	(12)
#define DE_PATH_GAMMA_IDX_OP_SEL_END_BIT	(13)
#define DE_PATH_GAMMA_IDX_INDEX_BEGIN_BIT	(0)
#define DE_PATH_GAMMA_IDX_INDEX_END_BIT		(7)

#define DE_PATH_GAMMA_RAM(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0030)

#define DE_PATH_CURSOR_FB(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0034)
#define DE_PATH_CURSOR_STR(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0038)

/* DE overlay registers */
#define DE_OVL_BASE				0x0400
#define DE_OVL_CFG(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0000)
#define DE_OVL_ISIZE(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0004)
#define DE_OVL_OSIZE(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0008)
#define DE_OVL_SR(n)				(DE_OVL_BASE + (n) * 0x100  +  0x000c)
#define DE_OVL_SCOEF0(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0010)
#define DE_OVL_SCOEF1(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0014)
#define DE_OVL_SCOEF2(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0018)
#define DE_OVL_SCOEF3(n)			(DE_OVL_BASE + (n) * 0x100  +  0x001c)
#define DE_OVL_SCOEF4(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0020)
#define DE_OVL_SCOEF5(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0024)
#define DE_OVL_SCOEF6(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0028)
#define DE_OVL_SCOEF7(n)			(DE_OVL_BASE + (n) * 0x100  +  0x002c)
#define DE_OVL_BA0(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0030)
#define DE_OVL_BA1UV(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0034)
#define DE_OVL_BA2V(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0038)
#define DE_OVL_3D_RIGHT_BA0(n)			(DE_OVL_BASE + (n) * 0x100  +  0x003C)
#define DE_OVL_3D_RIGHT_BA1UV(n)		(DE_OVL_BASE + (n) * 0x100  +  0x0040)
#define DE_OVL_3D_RIGHT_BA2V(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0044)
#define DE_OVL_STR(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0048)
#define DE_OVL_CRITICAL_CFG(n)			(DE_OVL_BASE + (n) * 0x100  +  0x004c)
#define DE_OVL_REMAPPING(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0050)
#define DE_OVL_CKMAX(n)				(DE_OVL_BASE + (n) * 0x100  +  0x005c)
#define DE_OVL_CKMIN(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0060)
#define DE_OVL_BLEND(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0064)

#define DE_OVL_COOR(m, n)			(DE_OVL_BASE + (n) * 0x100  +  0x0054)
#define DE_OVL_ALPHA_CFG(m, n)			(DE_OVL_BASE + (n) * 0x100  +  0x0058)
#define DE_OVL_ALPHA_ENABLE(m, n)		DE_OVL_BLEND(n)
#define DE_PATH_GAMMA_ENABLE(n)			DE_PATH_CTL(n)
#define DE_PATH_GAMMA_ENABLE_BIT		(9)

#define DE_OVL_CSC(n)				DE_OVL_CFG(n)
#define DE_OVL_CSC_CON_BEGIN_BIT		4
#define DE_OVL_CSC_CON_END_BIT			7
#define DE_OVL_CSC_STA_BEGIN_BIT		8
#define DE_OVL_CSC_STA_END_BIT			11
#define DE_OVL_CSC_BRI_BEGIN_BIT		12
#define DE_OVL_CSC_BRI_END_BIT			19
#define DE_OVL_CSC_BYPASS_BIT			0

#define DE_OVL_CFG_FLIP_BIT			20
#define DE_OVL_CFG_FMT_BEGIN_BIT		0
#define DE_OVL_CFG_FMT_END_BIT			2
#define DE_OVL_CFG_BYPASS_BIT			3
#define DE_OVL_CFG_CONTRAST_BEGIN_BIT       4
#define DE_OVL_CFG_CONTRAST_END_BIT         7
#define DE_OVL_CFG_SATURATION_BEGIN_BIT     8
#define DE_OVL_CFG_SATURATION_END_BIT       11
#define DE_OVL_CFG_LIGHTNESS_BEGIN_BIT      12
#define DE_OVL_CFG_LIGHTNESS_END_BIT        19
#define DE_OVL_CFG_CRITICAL_CTL_BEGIN_BIT	26
#define DE_OVL_CFG_CRITICAL_CTL_END_BIT		27

#define DE_OVL_ALPHA_CFG_PRE_MUTI_BIT		8
#define DE_OVL_ALPHA_CFG_VALUE_BEGIN_BIT	0
#define DE_OVL_ALPHA_CFG_VALUE_END_BIT		7
#define DE_OVL_ALPHA_CFG_ENABLE_BEGIN_BIT	0
#define DE_OVL_ALPHA_CFG_ENABLE_END_BIT		0

#define DE_OUTPUT_PATH1_DEVICE_BEGIN_BIT	0
#define DE_OUTPUT_PATH1_DEVICE_END_BIT		2
#define DE_OUTPUT_PATH2_DEVICE_BEGIN_BIT	4
#define DE_OUTPUT_PATH2_DEVICE_END_BIT		6

#define DE_PATH_CTL_IYUV_QEN_BIT		16
#define DE_PATH_CTL_YUV_FMT_BIT			15
#define DE_PATH_CTL_ILACE_BIT			11
#define DE_PATH_CTL_GAMMA_ENABLE_BIT		9

#define DE_PANEL_ENABLE_BIT			20
#define DE_PANEL_CURSOR_ENABLE_BIT	24
#define DE_PATH_FCR_BIT				29

#endif
