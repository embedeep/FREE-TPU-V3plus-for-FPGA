//-----------------------------------------------------------------------------
// The confidential and proprietary information contained in this file may
// only be used by a person authorised under and to the extent permitted
// by a subsisting licensing agreement from EMBEDEEP Corporation.
//
//    (C) COPYRIGHT 2018-2022  EMBEDEEP Corporation.
//        ALL RIGHTS RESERVED
//
// This entire notice must be reproduced on all copies of this file
// and copies of this file may only be made by a person if such person is
// permitted to do so under the terms of a subsisting license agreement
// from EMBEDEEP Corporation.
//
//      Main version        : V0.0.1
//
//      Revision            : 2022-10-08
//
//-----------------------------------------------------------------------------

#ifndef SRC_I2C_16BIT_H_
#define SRC_I2C_16BIT_H_

#include "xiicps.h"
#include "xil_types.h"
#include "config.h"

#define OV_CAM	0x3c

//DVP Register
#define DVPIN_CTRL_REG          (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1010))
#define DVPIN_STATUS_REG        (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1020))
#define DVPIN_ROW_WIDTH_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1030))
#define DVPIN_COL_WIDTH_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1040))
#define DVPIN_FRM_INFO_REG      (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1050))
#define DVPIN_DMA_ADDR1_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1060))
#define DVPIN_DMA_ADDR2_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1070))
#define DVPIN_DMA_ADDR3_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1080))
#define DVPOUT_CTRL_REG         (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1110))
#define DVPOUT_DMA_ADDR_REG     (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1120))
#define DVPOUT_STATUS_REG       (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1130))
#define DVPOUT_ROW_WIDTH_REG    (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1140))
#define DVPOUT_COL_WIDTH_REG    (*(volatile unsigned int *)(EEPDVP_REG_BASE_ADDR + 0x1150))

struct	config_table{
	u16	addr;
	u8	data;
};

int I2C_config_init_720p(unsigned int hsize, unsigned int vsize);
//int I2C_config_init_1080p(unsigned int hsize, unsigned int vsize);
int I2C_read(XIicPs *InstancePtr,u16 addr,u8 *read_buf);
int I2C_write(XIicPs *InstancePtr,u16 addr,u8 data);
u32 dvp_capture(volatile unsigned int cfg, unsigned int hsize, unsigned int vsize);
void RGB565toRGB888(u32 img_w, u32 img_h, u8 *img_data_565, u8 *img_data_888);
void GRAYtoRGB888(u32 img_w, u32 img_h, u8 *gray_data, u8 *img_data_888);
void dp_display(u32 img_w, u32 img_h, u8 *img_data_888, u32 *dp_buf);
void draw_object(u32 img_w, u32 x, u32 y, u32 w, u32 h, u8 *img_data_888);

#endif /* SRC_I2C_16BIT_H_ */
