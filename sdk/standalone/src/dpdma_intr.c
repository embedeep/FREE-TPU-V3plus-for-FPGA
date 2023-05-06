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

#include "dpdma_intr.h"

XDpDma DpDma;
XDpPsu DpPsu;
XAVBuf AVBuf;
Run_Config RunCfg;

XDpDma_FrameBuffer GFrameBuffer,VFrameBuffer;

#define GRAPHIC_BUFFER_EN
#define VIDEO_BUFFER_EN

/******************************************************************************/
/**
 * This function is called to wake-up the monitor from sleep.
 *
 * @param	RunCfgPtr is a pointer to the application configuration structure.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
static u32 DpPsu_Wakeup(Run_Config *RunCfgPtr)
{
	u32 Status;
	u8 AuxData;

	AuxData = 0x1;
	Status = XDpPsu_AuxWrite(RunCfgPtr->DpPsuPtr,
			XDPPSU_DPCD_SET_POWER_DP_PWR_VOLTAGE, 1, &AuxData);
	if (Status != XST_SUCCESS)
		xil_printf("\t! 1st power wake-up - AUX write failed.\n\r");
	Status = XDpPsu_AuxWrite(RunCfgPtr->DpPsuPtr,
			XDPPSU_DPCD_SET_POWER_DP_PWR_VOLTAGE, 1, &AuxData);
	if (Status != XST_SUCCESS)
		xil_printf("\t! 2nd power wake-up - AUX write failed.\n\r");

	return Status;
}

/******************************************************************************/
/**
 * This function is called to initiate training with the source device, using
 * the predefined source configuration as well as the sink capabilities.
 *
 * @param	RunCfgPtr is a pointer to the application configuration structure.
 *
 * @return	XST_SUCCESS if the function executes correctly.
 * 			XST_FAILURE if the function fails to execute correctly
 *
 * @note	None.
*******************************************************************************/
static u32 DpPsu_Hpd_Train(Run_Config *RunCfgPtr)
{
	XDpPsu		 *DpPsuPtr    = RunCfgPtr->DpPsuPtr;
	XDpPsu_LinkConfig *LinkCfgPtr = &DpPsuPtr->LinkConfig;
	u32 Status;

	Status = XDpPsu_GetRxCapabilities(DpPsuPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("\t! Error getting RX caps.\n\r");
		return XST_FAILURE;
	}

	Status = XDpPsu_SetEnhancedFrameMode(DpPsuPtr,
			LinkCfgPtr->SupportEnhancedFramingMode ? 1 : 0);
	if (Status != XST_SUCCESS) {
		xil_printf("\t! EFM set failed.\n\r");
		return XST_FAILURE;
	}

	Status = XDpPsu_SetLaneCount(DpPsuPtr,
			(RunCfgPtr->UseMaxLaneCount) ?
				LinkCfgPtr->MaxLaneCount :
				RunCfgPtr->LaneCount);
	if (Status != XST_SUCCESS) {
		xil_printf("\t! Lane count set failed.\n\r");
		return XST_FAILURE;
	}

	Status = XDpPsu_SetLinkRate(DpPsuPtr,
			(RunCfgPtr->UseMaxLinkRate) ?
				LinkCfgPtr->MaxLinkRate :
				RunCfgPtr->LinkRate);
	if (Status != XST_SUCCESS) {
		xil_printf("\t! Link rate set failed.\n\r");
		return XST_FAILURE;
	}

	Status = XDpPsu_SetDownspread(DpPsuPtr,
			LinkCfgPtr->SupportDownspreadControl);
	if (Status != XST_SUCCESS) {
		xil_printf("\t! Setting downspread failed.\n\r");
		return XST_FAILURE;
	}

	xil_printf("Lane count =\t%d\n\r", DpPsuPtr->LinkConfig.LaneCount);
	xil_printf("Link rate =\t%d\n\r",  DpPsuPtr->LinkConfig.LinkRate);

	// Link training sequence is done
        xil_printf("\n\r\rStarting Training...\n\r");
	Status = XDpPsu_EstablishLink(DpPsuPtr);
	if (Status == XST_SUCCESS)
		xil_printf("\t! Training succeeded.\n\r");
	else
		xil_printf("\t! Training failed.\n\r");

	return Status;
}


/******************************************************************************/
/**
 * This function will configure and establish a link with the receiver device,
 * afterwards, a video stream will start to be sent over the main link.
 *
 * @param	RunCfgPtr is a pointer to the application configuration structure.
 *
 * @return
 *		- XST_SUCCESS if main link was successfully established.
 *		- XST_FAILURE otherwise.
 *
 * @note	None.
 *
*******************************************************************************/
void DpPsu_Run(Run_Config *RunCfgPtr)
{
	u32 Status;
	XDpPsu  *DpPsuPtr = RunCfgPtr->DpPsuPtr;

	Status = InitDpDmaSubsystem(RunCfgPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("! InitDpDmaSubsystem failed.\n\r");
		return;
	}

	XDpPsu_EnableMainLink(DpPsuPtr, 0);

	if (!XDpPsu_IsConnected(DpPsuPtr)) {
		XDpDma_SetChannelState(RunCfgPtr->DpDmaPtr, GraphicsChan,
				       XDPDMA_DISABLE);
		xil_printf("! Disconnected.\n\r");
		return;
	}
	else {
		xil_printf("! Connected.\n\r");
	}

	Status = DpPsu_Wakeup(RunCfgPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("! Wakeup failed.\n\r");
		return;
	}


	u8 Count = 0;
	do {
		usleep(100000);
		Count++;

		Status = DpPsu_Hpd_Train(RunCfgPtr);
		if (Status == XST_DEVICE_NOT_FOUND) {
			xil_printf("Lost connection\n\r");
			return;
		}
		else if (Status != XST_SUCCESS)
			continue;

#ifdef GRAPHIC_BUFFER_EN
		XDpDma_DisplayGfxFrameBuffer(RunCfgPtr->DpDmaPtr, &GFrameBuffer);
#endif

#ifdef VIDEO_BUFFER_EN
		XDpDma_DisplayVideoFrameBuffer(RunCfgPtr->DpDmaPtr,&VFrameBuffer,NULL,NULL);
#endif
		DpPsu_SetupVideoStream(RunCfgPtr);
		XDpPsu_EnableMainLink(DpPsuPtr, 1);

		Status = XDpPsu_CheckLinkStatus(DpPsuPtr, DpPsuPtr->LinkConfig.LaneCount);
		if (Status == XST_DEVICE_NOT_FOUND)
			return;
	} while ((Status != XST_SUCCESS) && (Count < 2));
}

/******************************************************************************/
/**
 * This function is called when a Hot-Plug-Detect (HPD) event is received by the
 * DisplayPort TX core. The XDPPSU_INTERRUPT_STATUS_HPD_EVENT_MASK bit of the
 * core's XDPPSU_INTERRUPT_STATUS register indicates that an HPD event has
 * occurred.
 *
 * @param	InstancePtr is a pointer to the XDpPsu instance.
 *
 * @return	None.
 *
 * @note	Use the XDpPsu_SetHpdEventHandler driver function to set this
 *		function as the handler for HPD pulses.
 *
*******************************************************************************/
void DpPsu_IsrHpdEvent(void *ref)
{
	xil_printf("HPD event .......... ");
	DpPsu_Run((Run_Config *)ref);
	xil_printf(".......... HPD event\n\r");
}
/******************************************************************************/
/**
 * This function is called when a Hot-Plug-Detect (HPD) pulse is received by the
 * DisplayPort TX core. The XDPPSU_INTERRUPT_STATUS_HPD_PULSE_DETECTED_MASK bit
 * of the core's XDPPSU_INTERRUPT_STATUS register indicates that an HPD event has
 * occurred.
 *
 * @param	InstancePtr is a pointer to the XDpPsu instance.
 *
 * @return	None.
 *
 * @note	Use the XDpPsu_SetHpdPulseHandler driver function to set this
 *		function as the handler for HPD pulses.
 *
*******************************************************************************/
void DpPsu_IsrHpdPulse(void *ref)
{
	u32 Status;
	XDpPsu *DpPsuPtr = ((Run_Config *)ref)->DpPsuPtr;
	xil_printf("HPD pulse ..........\n\r");

	Status = XDpPsu_CheckLinkStatus(DpPsuPtr, DpPsuPtr->LinkConfig.LaneCount);
	if (Status == XST_DEVICE_NOT_FOUND) {
		xil_printf("Lost connection .......... HPD pulse\n\r");
		return;
	}

	xil_printf("\t! Re-training required.\n\r");

	XDpPsu_EnableMainLink(DpPsuPtr, 0);

	u8 Count = 0;
	do {
		Count++;

		Status = DpPsu_Hpd_Train((Run_Config *)ref);
		if (Status == XST_DEVICE_NOT_FOUND) {
			xil_printf("Lost connection .......... HPD pulse\n\r");
			return;
		}
		else if (Status != XST_SUCCESS) {
			continue;
		}

		DpPsu_SetupVideoStream((Run_Config *)ref);
		XDpPsu_EnableMainLink(DpPsuPtr, 1);

		Status = XDpPsu_CheckLinkStatus(DpPsuPtr, DpPsuPtr->LinkConfig.LaneCount);
		if (Status == XST_DEVICE_NOT_FOUND) {
			xil_printf("Lost connection .......... HPD pulse\n\r");
			return;
		}
	} while ((Status != XST_SUCCESS) && (Count < 2));

	xil_printf(".......... HPD pulse\n\r");
}


void DpPsu_SetupVideoStream(Run_Config *RunCfgPtr)
{
	XDpPsu		 *DpPsuPtr    = RunCfgPtr->DpPsuPtr;
	XDpPsu_MainStreamAttributes *MsaConfig = &DpPsuPtr->MsaConfig;

	XDpPsu_SetColorEncode(DpPsuPtr, RunCfgPtr->ColorEncode);
	XDpPsu_CfgMsaSetBpc(DpPsuPtr, RunCfgPtr->Bpc);
	XDpPsu_CfgMsaUseStandardVideoMode(DpPsuPtr, RunCfgPtr->VideoMode);

	/* Set pixel clock. */
	RunCfgPtr->PixClkHz = MsaConfig->PixelClockHz;
	XAVBuf_SetPixelClock(RunCfgPtr->PixClkHz);

	/* Reset the transmitter. */
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_SOFT_RESET, 0x1);
	usleep(10);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_SOFT_RESET, 0x0);

	XDpPsu_SetMsaValues(DpPsuPtr);
	/* Issuing a soft-reset (AV_BUF_SRST_REG). */
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, 0xB124, 0x3); // Assert reset.
	usleep(10);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, 0xB124, 0x0); // De-ssert reset.
	usleep(10);
	XDpPsu_EnableMainLink(DpPsuPtr, 1);

	xil_printf("DONE!\n\r");
}


void Dpdma_Setup_Intr_System(Run_Config *RunCfgPtr)
{
	XDpPsu 	 *DpPsuPtr = RunCfgPtr->DpPsuPtr;
	XScuGic	 *IntrPtr  = RunCfgPtr->IntrPtr;

	u32  IntrMask = XDPPSU_INTR_HPD_IRQ_MASK | XDPPSU_INTR_HPD_EVENT_MASK;

	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_DIS, 0xFFFFFFFF);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_MASK, 0xFFFFFFFF);

	XDpPsu_SetHpdEventHandler(DpPsuPtr, DpPsu_IsrHpdEvent, RunCfgPtr);
	XDpPsu_SetHpdPulseHandler(DpPsuPtr, DpPsu_IsrHpdPulse, RunCfgPtr);

	/* Register ISRs. */
	XScuGic_Connect(IntrPtr, DPPSU_INTR_ID,(Xil_InterruptHandler)XDpPsu_HpdInterruptHandler, RunCfgPtr->DpPsuPtr);

	/* Trigger DP interrupts on rising edge. */
	XScuGic_SetPriorityTriggerType(IntrPtr, DPPSU_INTR_ID, 0x0, 0x03);

	/* Connect DPDMA Interrupt */
	XScuGic_Connect(IntrPtr, DPDMA_INTR_ID,(Xil_ExceptionHandler)XDpDma_InterruptHandler, RunCfgPtr->DpDmaPtr);

	/* Enable DP interrupts. */
	XScuGic_Enable(IntrPtr, DPPSU_INTR_ID);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_EN, IntrMask);

	/* Enable DPDMA Interrupts */
	XScuGic_Enable(IntrPtr, DPDMA_INTR_ID);
	XDpDma_InterruptEnable(RunCfgPtr->DpDmaPtr, XDPDMA_IEN_VSYNC_INT_MASK);
}


int InitDpDmaSubsystem(Run_Config *RunCfgPtr )
{
	u32 Status;
	XDpPsu		*DpPsuPtr = RunCfgPtr->DpPsuPtr;
	XDpPsu_Config	*DpPsuCfgPtr;
	XAVBuf		*AVBufPtr = RunCfgPtr->AVBufPtr;
	XDpDma_Config *DpDmaCfgPtr;
	XDpDma		*DpDmaPtr = RunCfgPtr->DpDmaPtr;


	/* Initialize DisplayPort driver. */
	DpPsuCfgPtr = XDpPsu_LookupConfig(DPPSU_DEVICE_ID);
	XDpPsu_CfgInitialize(DpPsuPtr, DpPsuCfgPtr, DpPsuCfgPtr->BaseAddr);
	/* Initialize Video Pipeline driver */
	XAVBuf_CfgInitialize(AVBufPtr, DpPsuPtr->Config.BaseAddr, AVBUF_DEVICE_ID);

	/* Initialize the DPDMA driver */
	DpDmaCfgPtr = XDpDma_LookupConfig(DPDMA_DEVICE_ID);
	XDpDma_CfgInitialize(DpDmaPtr,DpDmaCfgPtr);

	/* Initialize the DisplayPort TX core. */
	Status = XDpPsu_InitializeTx(DpPsuPtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Set the format graphics frame for DPDMA*/
	Status = XDpDma_SetVideoFormat(DpDmaPtr, RGBA8880);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}

	/* Set the format graphics frame for DPDMA*/
	Status = XDpDma_SetGraphicsFormat(DpDmaPtr, RGBA8888);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}

	Status = XAVBuf_SetInputNonLiveVideoFormat(AVBufPtr, RGBA8880);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}
	/* Set the format graphics frame for Video Pipeline*/
	Status = XAVBuf_SetInputNonLiveGraphicsFormat(AVBufPtr, RGBA8888);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}

	/* Set the QOS for Video */
	XDpDma_SetQOS(RunCfgPtr->DpDmaPtr, 11);
	/* Enable the Buffers required by Graphics Channel */
	XAVBuf_EnableVideoBuffers(RunCfgPtr->AVBufPtr, 1);
	XAVBuf_EnableGraphicsBuffers(RunCfgPtr->AVBufPtr, 1);
	/* Set the output Video Format */
	XAVBuf_SetOutputVideoFormat(AVBufPtr, RGB_8BPC);

	/* Select the Input Video Sources.
	 * Here in this example we are going to demonstrate
	 * graphics overlay over the TPG video.
	 */
	XAVBuf_InputVideoSelect(AVBufPtr, XAVBUF_VIDSTREAM1_NONLIVE,XAVBUF_VIDSTREAM2_NONLIVE_GFX);
	/* Configure Video pipeline for graphics channel */
	XAVBuf_ConfigureVideoPipeline(AVBufPtr);
	XAVBuf_ConfigureGraphicsPipeline(AVBufPtr);
	/* Configure the output video pipeline */
	XAVBuf_ConfigureOutputVideo(AVBufPtr);
	/* Disable the global alpha, since we are using the pixel based alpha */
	XAVBuf_SetBlenderAlpha(AVBufPtr, 0, 0);
	/* Set the clock mode */
	XDpPsu_CfgMsaEnSynchClkMode(DpPsuPtr, RunCfgPtr->EnSynchClkMode);
	/* Set the clock source depending on the use case.
	 * Here for simplicity we are using PS clock as the source*/
	XAVBuf_SetAudioVideoClkSrc(AVBufPtr, XAVBUF_PS_CLK, XAVBUF_PS_CLK);
	/* Issue a soft reset after selecting the input clock sources */
	XAVBuf_SoftReset(AVBufPtr);

	return XST_SUCCESS;
}

void graphic_buffer_update(u8* Frame, Run_Config *RunCfgPtr)
{
	GFrameBuffer.Address = (INTPTR)Frame;
	GFrameBuffer.Stride = STRIDE;
	GFrameBuffer.LineSize = LINESIZE;
	GFrameBuffer.Size = BUFFERSIZE;
	XDpDma_DisplayGfxFrameBuffer(RunCfgPtr->DpDmaPtr, &GFrameBuffer);
	return ;
}


void graphic_buffer_init(u8* Frame)
{
	u32 vcnt,hcnt;
	u32 *RGBA;
	RGBA = (u32 *) Frame;

	for(vcnt =0 ; vcnt < 720 ; vcnt++)
	{

		for(hcnt = 0; hcnt < 1280; hcnt++)
			{

				if((hcnt&0x10)^(vcnt&0x10))
					RGBA[hcnt+vcnt*1280] = 0x00000000;//Alpha = 0x00
				else
					RGBA[hcnt+vcnt*1280] = 0xffffffff;//Alpha = 0xff
			}

	}
	/* Populate the FrameBuffer structure with the frame attributes */
	GFrameBuffer.Address = (INTPTR)Frame;
	GFrameBuffer.Stride = STRIDE;
	GFrameBuffer.LineSize = LINESIZE;
	GFrameBuffer.Size = BUFFERSIZE;
	return ;
}


void video_buffer_update(u8* Frame, Run_Config *RunCfgPtr)
{
	VFrameBuffer.Address = (INTPTR)Frame;
	VFrameBuffer.Stride = STRIDE;
	VFrameBuffer.LineSize = LINESIZE;
	VFrameBuffer.Size = BUFFERSIZE;
	XDpDma_DisplayVideoFrameBuffer(RunCfgPtr->DpDmaPtr,&VFrameBuffer,NULL,NULL);
	return ;
}

void video_buffer_init(u8* Frame)
{
	u32 vcnt,hcnt;
	u32 *RGBA;
	RGBA = (u32 *) Frame;


	for(vcnt =0 ; vcnt < 720 ; vcnt++)
	{

		for(hcnt = 0; hcnt < 1280; hcnt++)
			{
				RGBA[hcnt+vcnt*1280] = 0x000000FF;
			}

	}
	/* Populate the FrameBuffer structure with the frame attributes */
	VFrameBuffer.Address = (INTPTR)Frame;
	VFrameBuffer.Stride = STRIDE;
	VFrameBuffer.LineSize = LINESIZE;
	VFrameBuffer.Size = BUFFERSIZE;
	return ;
}

void InitRunConfig(Run_Config *RunCfgPtr ,XScuGic * IntcPtr)
{
	/* Initial configuration parameters. */
	RunCfgPtr->DpPsuPtr   			= &DpPsu;
	RunCfgPtr->IntrPtr   			= IntcPtr;
	RunCfgPtr->AVBufPtr  			= &AVBuf;
	RunCfgPtr->DpDmaPtr  			= &DpDma;
	RunCfgPtr->VideoMode 			= XVIDC_VM_1280x720_60_P;
	RunCfgPtr->Bpc		 			= XVIDC_BPC_8;
	RunCfgPtr->ColorEncode			= XDPPSU_CENC_RGB;
	RunCfgPtr->UseMaxCfgCaps		= 1;
	RunCfgPtr->LaneCount			= LANE_COUNT_2;
	RunCfgPtr->LinkRate				= LINK_RATE_540GBPS;
	RunCfgPtr->EnSynchClkMode		= 0;
	RunCfgPtr->UseMaxLaneCount		= 1;
	RunCfgPtr->UseMaxLinkRate		= 1;

}

int Dpdma_init(Run_Config *RunCfgPtr ,XScuGic * IntcPtr)
{
	u32 Status;
	/* Initialize the application configuration */
	InitRunConfig(RunCfgPtr , IntcPtr);
	Status = InitDpDmaSubsystem(RunCfgPtr);
	if (Status != XST_SUCCESS) {
				return XST_FAILURE;
	}
	return XST_SUCCESS;
}
