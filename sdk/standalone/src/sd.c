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

#include "sd.h"

#include "ff.h"
//#include "xdevcfg.h"
#include "xstatus.h"
#include "xil_printf.h"

static FATFS fatfs;
unsigned char read_line_buf[1920 * 3];
unsigned char Write_line_buf[1920 * 3];

int SD_Init()
{
    FRESULT rc;

    rc = f_mount(&fatfs,"",0);
    if(rc)
    {
        xil_printf("ERROR : f_mount returned %d\r\n",rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

void bmp_read(char * bmp,unsigned char *frame,unsigned int stride)
{
	FIL fil;
	short y,x;
	short Ximage;
	short Yimage;
	u32 iPixelAddr = 0;
	FRESULT res;
	unsigned char TMPBUF[64];
	unsigned int br;         // File R/W count

	res = f_open(&fil, bmp, FA_OPEN_EXISTING | FA_READ);
	if(res != FR_OK)
	{
		return ;
	}
	res = f_read(&fil, TMPBUF, 54, &br);
	if(res != FR_OK)
	{
		return ;
	}
	Ximage=(unsigned short int)TMPBUF[19]*256+TMPBUF[18];
	Yimage=(unsigned short int)TMPBUF[23]*256+TMPBUF[22];
	iPixelAddr = (Yimage-1)*stride ;

	for(y = 0; y < Yimage ; y++)
	{
		f_read(&fil, read_line_buf, Ximage * 3, &br);
		for(x = 0; x < Ximage; x++)
		{
			frame[x * BYTES_PIXEL + iPixelAddr + 0] = read_line_buf[x * 3 + 0];
			frame[x * BYTES_PIXEL + iPixelAddr + 1] = read_line_buf[x * 3 + 1];
			frame[x * BYTES_PIXEL + iPixelAddr + 2] = read_line_buf[x * 3 + 2];
		}
		iPixelAddr -= stride;
	}
	f_close(&fil);
}


void bmp_write(char * name, char *head_buf, char *data_buf)
{
	FIL fil;
	short y,x;
	short Ximage;
	short Yimage;
	u32 iPixelAddr = 0;
	FRESULT res;
	unsigned int br;         // File R/W count

	memset(&Write_line_buf, 0, 1920*3) ;

	res = f_open(&fil, name, FA_CREATE_ALWAYS | FA_WRITE);
	if(res != FR_OK)
	{
		return ;
	}
	res = f_write(&fil, head_buf, 54, &br) ;
	if(res != FR_OK)
	{
		return ;
	}
	Ximage=(unsigned short)head_buf[19]*256+head_buf[18];
	Yimage=(unsigned short)head_buf[23]*256+head_buf[22];
	iPixelAddr = (Yimage-1)*Ximage*3 ;
	for(y = 0; y < Yimage ; y++)
	{
		for(x = 0; x < Ximage; x++)
		{
			Write_line_buf[x*3 + 0] = data_buf[x*3 + iPixelAddr + 0] ;
			Write_line_buf[x*3 + 1] = data_buf[x*3 + iPixelAddr + 1] ;
			Write_line_buf[x*3 + 2] = data_buf[x*3 + iPixelAddr + 2] ;
		}
		res = f_write(&fil, Write_line_buf, Ximage*3, &br) ;
		if(res != FR_OK)
		{
			return ;
		}
		iPixelAddr -= Ximage*3;
	}

	f_close(&fil);
}

int file_read(char * bmp,unsigned char *frame, unsigned int len)
{
	FIL fil;
	FRESULT res;
	unsigned int br;         // File R/W count
	unsigned int line;

	res = f_open(&fil, bmp, FA_OPEN_EXISTING | FA_READ);
	if(res != FR_OK)
	{
		xil_printf("open file fail!\n");
		return XST_FAILURE;
	}
//	res = f_lseek($fil,)
	res = f_read(&fil, (void*)frame, len, &br);
	if(res != FR_OK)
	{
		xil_printf("read file fail!\n");
		return XST_FAILURE;
	}

	f_close(&fil);
	return XST_SUCCESS;
}


int file_write(char * name, char *head_buf, int len)
{
	FIL fil;
	FRESULT res;
	unsigned int br;         // File R/W count


	res = f_open(&fil, name, FA_CREATE_ALWAYS | FA_WRITE);
	if(res != FR_OK)
	{
		xil_printf("open write file fail!\n");
		return XST_FAILURE;
	}
	res = f_write(&fil, head_buf, len, &br) ;
	if(res != FR_OK)
	{
		xil_printf("write to file fail!\n");
		return XST_FAILURE;
	}

	f_close(&fil);
	return XST_SUCCESS;
}
