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

#include <stdio.h>
#include <unistd.h>
#include "eep_interface.h"

EEP_INTERFACE::EEP_INTERFACE()
{
    mem_base_addr = 0;
    tpu_reg_base = 0;
}

EEP_INTERFACE::~EEP_INTERFACE() { }



int EEP_INTERFACE::mem_write(unsigned long addr, unsigned long loadlen, unsigned char *datbuf, unsigned long buf_size)
{
    volatile unsigned char* pdst = (volatile unsigned char*)addr;
    unsigned char* psrc = datbuf;

    for (unsigned int i = 0; i < buf_size; i++)
    {
        *pdst++ = *psrc++;
    }

    return 0;
}

int EEP_INTERFACE::mem_read(unsigned long addr, unsigned char* readbuf, unsigned long readlen)
{
    volatile unsigned char* psrc = (volatile unsigned char*)addr;
    unsigned char* pdst = readbuf;

    for (unsigned int i = 0; i < readlen; i++)
    {
        *pdst++ = *psrc++;
    }

    return 0;
}

int EEP_INTERFACE::register_write(unsigned long addr, unsigned int wr_val)
{
	printf("register write: addr 0x%08x, val 0x%08x\r\n", addr, wr_val);
    *(volatile unsigned int*)addr = wr_val;
    return 0;
}

int EEP_INTERFACE::register_read(unsigned long addr, unsigned int* rd_val)
{
    *rd_val = *(volatile unsigned int*)addr;
    //printf("register read: addr 0x%08x, val 0x%08x\r\n", addr, *rd_val);
    return 0;
}

int EEP_INTERFACE::register_wait(unsigned long addr, unsigned int mask, unsigned int want_val)
{
    int ret;
    unsigned int rval = 0;

    printf("reg waiting: addr 0x%08x, want 0x%08x\r\n", addr, want_val);

    while(1)
    {
        register_read(addr, &rval);
        if ((rval & mask) == want_val)
		{
			ret = 0;
			break;
		}
        //usleep(1000);
//        register_read(0xa0000020, &rval);
//        printf("layer cnt: %d\r\n",rval);
    }

    return ret;
}




