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

#ifndef SRC_CONFIG_H_
#define SRC_CONFIG_H_


// -------- EEPTPU Config --------
#define    EEPTPU_MEM_BASE_ADDR		0x31000000
#define    EEPTPU_REG_BASE_ADDR     0xA0000000
#define    EEPDVP_REG_BASE_ADDR     0xA00C0000
#define    EEPTPU_RUNTIMER_REG      (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x24))
#define    EEPTPU_BASEADDR0_REG     (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x50))
#define    EEPTPU_BASEADDR1_REG     (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x54))
#define    EEPTPU_BASEADDR2_REG     (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x58))
#define    EEPTPU_BASEADDR3_REG     (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x5C))
#define    EEPTPU_ALGOADDR_REG      (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x30))
#define    EEPTPU_STARTUP_REG       (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0x34))
#define    EEPTPU_STATUS_REG        (*(volatile unsigned int *)(EEPTPU_REG_BASE_ADDR+0xC))

// 1-use eepnet.h and eepinput.h;   0-all in eepnet.h
#define    FG_INPUT_DATA_SEPERATED  1

#define    NetType_Classify   				0	// e.g. lenet, mobilenet, resnet, inception, squeezenet
#define    NetType_Object_Detect   			1	// e.g. mobilenet-ssd, mobilenet-yolo
#define    NetType_Semantic_Segmentation   	2	// e.g. ICNET

//#define     NET_TYPE        NetType_Classify
#define     NET_TYPE      NetType_Object_Detect
#define     NET_SIZE      12240064
#define     INPUTDATA_SIZE 5537792

// -------- Post process --------
#if (NET_TYPE == NetType_Object_Detect)
    #define     YOLO3_DETECTION_OUTPUT       1       // only for yolo3
#endif

// -------- debug information print ----------
//#define EEP_DEBUG_INFO

// -------- Camera Settings -----------
#define EEP_DVP_CAMERA
#define IMG_RGB565 // set input image type: rgb or gray
#define IMG_HEIGHT 720
#define IMG_WIDTH  1280

// ------- SD card Enable -------------
#define SD_CARD_IS_READY

// ------- SD card Enable -------------
#define EEP_DP_ENABLE


#endif /* SRC_CONFIG_H_ */
