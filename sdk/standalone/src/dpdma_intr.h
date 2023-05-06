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

#ifndef DP_DMA_INTR_H_
#define DP_DMA_INTR_H_

/******************************* Include Files ********************************/
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xparameters.h"	/* SDK generated parameters */
#include "xdpdma.h"			/* DPDMA device driver */
#include "xscugic.h"		/* Interrupt controller device driver */
#include "xdppsu.h"			/* DP controller device driver */
#include "xavbuf.h"    		/* AVBUF is the video pipeline driver */
#include "xavbuf_clk.h"		/* Clock Driver for Video(VPLL) and Audio(RPLL) clocks */


/************************** Constant Definitions *****************************/
#define DPPSU_DEVICE_ID		XPAR_PSU_DP_DEVICE_ID
#define AVBUF_DEVICE_ID		XPAR_PSU_DP_DEVICE_ID
#define DPDMA_DEVICE_ID		XPAR_XDPDMA_0_DEVICE_ID
#define DPPSU_INTR_ID		151
#define DPDMA_INTR_ID		154
#define INTC_DEVICE_ID		XPAR_SCUGIC_0_DEVICE_ID

#define DPPSU_BASEADDR		XPAR_PSU_DP_BASEADDR
#define AVBUF_BASEADDR		XPAR_PSU_DP_BASEADDR
#define DPDMA_BASEADDR		XPAR_PSU_DPDMA_BASEADDR

#define BUFFERSIZE			1280* 720 * 4		/* HTotal * VTotal * BPP */
#define LINESIZE			1280 * 4			/* HTotal * BPP */
#define STRIDE				LINESIZE			/* The stride value should
													be aligned to 256*/
typedef enum {
	LANE_COUNT_1 = 1,
	LANE_COUNT_2 = 2,
} LaneCount_t;

typedef enum {
	LINK_RATE_162GBPS = 0x06,
	LINK_RATE_270GBPS = 0x0A,
	LINK_RATE_540GBPS = 0x14,
} LinkRate_t;

typedef struct {
	XDpPsu	*DpPsuPtr;
	XScuGic	*IntrPtr;
	XAVBuf	*AVBufPtr;
	XDpDma	*DpDmaPtr;

	XVidC_VideoMode	  VideoMode;
	XVidC_ColorDepth  Bpc;
	XDpPsu_ColorEncoding ColorEncode;

	u8 UseMaxLaneCount;
	u8 UseMaxLinkRate;
	u8 LaneCount;
	u8 LinkRate;
	u8 UseMaxCfgCaps;
	u8 EnSynchClkMode;

	u32 PixClkHz;
} Run_Config;


/************************** Variable Declarations ***************************/

int Dpdma_init(Run_Config *RunCfgPtr ,XScuGic * IntcPtr);
void InitRunConfig(Run_Config *RunCfgPtr ,XScuGic * IntcPtr);
int  InitDpDmaSubsystem(Run_Config *RunCfgPtr);

void graphic_buffer_update(u8* Frame, Run_Config *RunCfgPtr);
void graphic_buffer_init(u8* Frame);

void video_buffer_update(u8* Frame, Run_Config *RunCfgPtr);
void video_buffer_init(u8* Frame);

void Dpdma_Setup_Intr_System(Run_Config *RunCfgPtr);
void DpPsu_SetupVideoStream(Run_Config *RunCfgPtr);
void DpPsu_Run(Run_Config *RunCfgPtr);
void DpPsu_IsrHpdEvent(void *ref);
void DpPsu_IsrHpdPulse(void *ref);

/************************** Variable Definitions *****************************/


#endif /* SRC_DPDMA_VIDEO_EXAMPLE_H_ */
