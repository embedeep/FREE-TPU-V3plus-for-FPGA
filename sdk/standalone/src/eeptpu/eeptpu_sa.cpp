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
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "eeptpu_sa.h"
#include "../utils.h"
#include "../config.h"

EEPTPU_SA::EEPTPU_SA()
{
    //tpu_hwbase_count = 2;
    //tpu_hwbase = (unsigned int*)malloc(tpu_hwbase_count * sizeof(unsigned int));
    //memset(tpu_hwbase, 0, tpu_hwbase_count * sizeof(unsigned int));
    b_terminate = false;

    b_ex = false;

    interface = 0;
    mem_base = 0;
    hwbase0 = 0;
    hwbase1 = 0;
    memsize = 0;
    tpureg_addr = 0;
    reg_size = 0;

    bin_type = 1;   // enc=1; pub=2
}
EEPTPU_SA::~EEPTPU_SA()
{
    //if (tpu_hwbase != NULL) free(tpu_hwbase);
}

static char tpuver_str[32] = {0};
char* eep_get_tpu_version(unsigned int rdval)
{
    // hw_v2[5:0].hw_v1[5:0].hw_v0[5:0].hw_r0[5:0].hw_p0[7:0]
    unsigned int v1 = (rdval >> 26) & 0x3f;
    unsigned int v2 = (rdval >> 20) & 0x3f;
    unsigned int v3 = (rdval >> 14) & 0x3f;
    unsigned int r = (rdval >> 8) & 0x3f;
    unsigned int p = (rdval >> 0) & 0xff;
    sprintf(tpuver_str, "%d.%d.%d-r%d-p%d", v1, v2, v3, r, p);
    return tpuver_str;
}


int EEPTPU_SA::mem_load_from_ram(unsigned int addr, unsigned char* data, int datalen)
{
    int ret = -1;

    // load data to mem.
    memhwaddr_start = addr;
    printf("write net data: addr 0x%08x, len 0x%x\r\n", memhwaddr_start, datalen);
    ret = eepif.mem_write(memhwaddr_start, datalen, data, datalen);
    if (ret == 0) printf("Mem load succ.\n");
    else printf("Mem load fail, ret = %d.\n", ret);

#if 0
    if (ret == 0)
    {
    	unsigned char buff[1024];
    	//ret = eepif.mem_read(memhwaddr_start+datalen - sizeof(buff), buff, sizeof(buff));
    	ret = eepif.mem_read(0x700e1000, buff, sizeof(buff));
    	if (ret == 0) printf("mem read ok\r\n");
    	else printf("mem read fail\r\n");

    	int c = 0;
    	for (int i = 0; i < sizeof(buff); i++)
    	{
    		printf("0x%02X ", buff[i]);
    		c++;
    		if (c == 16) { printf("\r\n"); c = 0; }
    	}
    }
#endif

    return ret;
}

// init from ram data.
// read config from param 'config', write net 'data' to memory.
int EEPTPU_SA::eeptpu_init(unsigned char* data, int datalen, unsigned char* config, int cfglen)
{
    int ret = -1;

    /* Format:  for lib enc/pub
                interface(int*1), mem_base(int*1), tpureg_addr(int*1), reg_size(int*1, if not use, set 0),
                bin_type(int*1; enc=1; pub=2),
                mem_cnt(int*1), base_ofs0(int*1), ..., base_ofsN(int*1), mem_size(int*1), CntOut(int*1),
                DataOut0:  ofs(int*1), shape[0](int*1), shape[1](int*1), shape[2](int*1), shape[3](int*1), exp(int*1),
                DataOut1(if exist): ..... ,
                DataAlg_addr(int*1),
                DataIn:  ofs(int*1), shape[0](int*1), shape[1](int*1), shape[2](int*1), shape[3](int*1), exp(int*1),
                mean_count(int*1),mean_list(fp32*N),norm_list(fp32*N)
            */
    unsigned int* pcfg = (unsigned int*) config;
    if (cfglen < 8*4) return -1;
    //interface = *pcfg++;
    //mem_base = *pcfg++;
    //tpureg_addr = *pcfg++;
    //reg_size = *pcfg++;
    pcfg+=4;	// bare-metal mode, not use interface; set mem/reg base addr in src.
    mem_base = EEPTPU_MEM_BASE_ADDR;
    tpureg_addr = EEPTPU_REG_BASE_ADDR;
    reg_size = 0;	// ignore

    bin_type = *pcfg++;
    mem_cnt = *pcfg++;
    if (bin_type == 1)
    {
        hwbase0 = (*pcfg++) + mem_base;
        hwbase1 = (*pcfg++) + mem_base;
    }
    else if (bin_type == 2)
    {
        hwbase0 = (*pcfg++) + mem_base;
        hwbase1 = (*pcfg++) + mem_base;
        hwbase2 = (*pcfg++) + mem_base;
        hwbase3 = (*pcfg++) + mem_base;
    }

    memsize = *pcfg++;
    int cnt_out = *pcfg++;
    printf("output cnt = %d\n", cnt_out);
    //if (cfglen < ( 8*4 + cnt_out*6*4 + 1*4 ) ) return -1;
    for (int i = 0; i < cnt_out; i++)
    {
        struct st_hwaddr_info info;
        info.hwaddr = (*pcfg++) + mem_base;
        info.shape[0] = *pcfg++;
        info.shape[1] = *pcfg++;
        info.shape[2] = *pcfg++;
        info.shape[3] = *pcfg++;
        info.exp = *pcfg++;
        printf("out[%d]: hwaddr 0x%08x, shape: %d,%d,%d,%d\n", i, info.hwaddr, info.shape[0], info.shape[1], info.shape[2], info.shape[3]);
        addr_out.push_back(info);
    }
    int cnt_threads = *pcfg++;
    addr_alg = (*pcfg++) + mem_base;
    *pcfg++;
    {
		addr_in.hwaddr = (*pcfg++) + mem_base;
		addr_in.shape[0] = *pcfg++;
		addr_in.shape[1] = *pcfg++;
		addr_in.shape[2] = *pcfg++;
		addr_in.shape[3] = *pcfg++;
		addr_in.exp = *pcfg++;
		printf("in: hwaddr 0x%08x, shape: %d,%d,%d,%d\n", addr_in.hwaddr, addr_in.shape[0], addr_in.shape[1], addr_in.shape[2], addr_in.shape[3]);
	}
    int in_ch = *pcfg++;  mean.clear(); norm.clear();
    for (int i = 0; i < in_ch; i++) { mean.push_back(*(float*)pcfg); pcfg++; }
    for (int i = 0; i < in_ch; i++) { norm.push_back(*(float*)pcfg); pcfg++; }

    // ------ read config done.
    //printf("interface = %d\n", interface);
    printf("mem_base = 0x%08lx\n", mem_base);
    printf("tpureg_addr = 0x%08lx\n", tpureg_addr);
    //printf("reg_size = 0x%08lx\n", reg_size);
    printf("hwbase0 = 0x%08lx\n", hwbase0);
    printf("hwbase1 = 0x%08lx\n", hwbase1);
    printf("hwbase2 = 0x%08lx\n", hwbase2);
    printf("hwbase3 = 0x%08lx\n", hwbase3);
    printf("memsize = 0x%08lx\n", memsize);
    printf("addr_alg = 0x%08lx\n", addr_alg);

    eepif.mem_base_addr = mem_base;
    eepif.tpu_reg_base = tpureg_addr;

    unsigned int rval;
    ret = eepreg_read(0x44, &rval); if (ret < 0) return ret;
    printf("TPU hardware version: 0x%08x (%s)\n", rval, eep_get_tpu_version(rval));

    // load data.
    printf("Loading data\n");

    unsigned int wraddr;
#if (FG_INPUT_DATA_SEPERATED == 1)
    // load net data. ( not include input data)
    wraddr = hwbase1;
    //ret = mem_load_from_ram(hwbase1, data, datalen);
    //if (ret < 0) return ret;
#else
	wraddr = hwbase0;
    //ret = mem_load_from_ram(hwbase0, data, datalen);
    //if (ret < 0) return ret;
#endif
#ifndef SD_CARD_IS_READY
    printf("write net data: addr 0x%08x, len 0x%x\r\n", wraddr, datalen);
    ret = eepif.mem_write(wraddr, datalen, data, datalen);
    if (ret == 0) printf("Mem load succ.\n");
    else printf("Mem load fail, ret = %d.\n", ret);
#endif
    
    return 0;
}

// write input data to memory
int EEPTPU_SA::eeptpu_input(unsigned char* input_data, int datalen)
{
#if (FG_INPUT_DATA_SEPERATED == 1)
    // load input data (eepinput) to mem.
    printf("write input data: addr 0x%08x, len 0x%x\r\n", hwbase1, datalen);
    int ret = eepif.mem_write(hwbase1, datalen, input_data, datalen);
    if (ret == 0) printf("Mem load succ.\n");
    else printf("Mem load fail, ret = %d.\n", ret);
    return ret;
#else
    return 0;
#endif
}

int EEPTPU_SA::eeptpu_deinit()
{
    return 0;
}

// eeptpu register read. use register offset.
int EEPTPU_SA::eepreg_read(unsigned int regaddr, unsigned int* regval)
{
    return eepif.register_read(tpureg_addr | regaddr, regval);
}

// eeptpu register write. use register offset.
int EEPTPU_SA::eepreg_write(unsigned int regaddr, unsigned int regval)
{
    return eepif.register_write(tpureg_addr | regaddr, regval);
}

// eeptpu register wait. use register offset.
int EEPTPU_SA::eepreg_wait(unsigned long regaddr, unsigned int mask, unsigned int want_val)
{
    return eepif.register_wait(tpureg_addr | regaddr, mask, want_val);
}

int EEPTPU_SA::eep_tpu_start_work(unsigned long addr)
{
    int ret;

    if (bin_type == 1)
    {
        ret = eepreg_write(0x00000050, hwbase0 & 0xFFFFFFFF);  if (ret < 0) return ret;
        ret = eepreg_write(0x00000054, hwbase1 & 0xFFFFFFFF);  if (ret < 0) return ret;
    }
    else if (bin_type == 2)
    {
        ret = eepreg_write(0x00000050, hwbase0 & 0xFFFFFFFF);  if (ret < 0) return ret;     // par
        ret = eepreg_write(0x00000054, hwbase1 & 0xFFFFFFFF);  if (ret < 0) return ret;     // in
        ret = eepreg_write(0x00000058, hwbase2 & 0xFFFFFFFF);  if (ret < 0) return ret;     // tmp
        ret = eepreg_write(0x0000005c, hwbase3 & 0xFFFFFFFF);  if (ret < 0) return ret;     // out
    }
    ret = eepreg_write(0x00000030, addr);       if (ret < 0) return ret;
    ret = eepreg_write(0x00000034, 0x00000011); if (ret < 0) return ret;

    /*
    unsigned int rval = 0;
    eepreg_read(0x50, &rval);  printf("Read ret 0x50: 0x%08x\r\n", rval);
    eepreg_read(0x54, &rval);  printf("Read ret 0x54: 0x%08x\r\n", rval);
    eepreg_read(0x30, &rval);  printf("Read ret 0x30: 0x%08x\r\n", rval);
    eepreg_read(0x34, &rval);  printf("Read ret 0x34: 0x%08x\r\n", rval);
    */

    return 0;
}

int EEPTPU_SA::eep_tpu_wait_done()
{
    int ret;
    ret = eepreg_wait(0x0000000c, 0x80000000, 0x80000000);
    if (ret < 0) return ret;

    return ret;
}


int EEPTPU_SA::forward()
{
    int ret;

    ret = eep_tpu_start_work(addr_alg);
    if (ret < 0) return ret;

    ret = eep_tpu_wait_done();
    if (ret < 0) return ret;

    return 0;
}

// convert eeptpu data to ncnn mat.
int epmat_get_size(int c, int h, int w)
{
    if (h == 1 && w == 1) return round_up(c, 16) * 2;
    else return (h * w * round_up(c, 16)  * 2);
}
ncnn::Mat epmat2nmat_simple(int channel, int height, int width, unsigned char* epmat, int exp)
{
    ncnn::Mat dstmat = ncnn::Mat(width);
    float* pdst = (float*)dstmat;
    int div_val = pow(2, exp);
    unsigned char* p_epmat = epmat;
    for (int w=0; w < channel * height * width; w++)
    {
        short tmpval = *p_epmat++;
        tmpval |= (*p_epmat) << 8;
        p_epmat++;
        *(pdst + w) = (float)tmpval / div_val;
    }

    return dstmat;
}
ncnn::Mat epmat2nmat(int channel, int height, int width, unsigned char *epmat, int exp)
{
    if (epmat == NULL) return ncnn::Mat();
    if (height == 1 && channel == 1)
    {
        return epmat2nmat_simple(channel, height, width, epmat, exp);
    }

    ncnn::Mat dstmat = ncnn::Mat(width, height, channel);

    int ch_group = channel / 16;
    if (channel % 16) ch_group++;
    int epmat_ch_group = height * width * 16;
    int div_val = pow(2, exp);

    for (int c=0; c < channel; c++)
    {
        int curr_ch_group = c / 16;
        float* pdst = (float*)dstmat.channel(c).data;
        int epmat_offset = (curr_ch_group * epmat_ch_group + (c % 16)) * 2;

        for (int hw=0; hw < height * width; hw++)
        {
            short tmpval =    *(epmat + epmat_offset + 0);
            tmpval |= ((*(epmat + epmat_offset + 1)) << 8);
            *pdst++ = (float)tmpval / div_val;
            epmat_offset += 32;
        }
    }

    return dstmat;
}

// read forward result from memory, convert it to ncnn mat format.
int EEPTPU_SA::read_forward_result(vector< ncnn::Mat >& outputs)
{
    int ret = -1;
    for (unsigned int i = 0; i < addr_out.size(); i++)
    {
        ncnn::Mat out;
        printf("read output[%d]: addr 0x%08x, shape: %d,%d,%d\n", i, addr_out[i].hwaddr, addr_out[i].shape[1], addr_out[i].shape[2], addr_out[i].shape[3]);
        unsigned int epmat_size = epmat_get_size(addr_out[i].shape[1], addr_out[i].shape[2], addr_out[i].shape[3]);
        printf("epmat_size = 0x%x(%d)\n", epmat_size, epmat_size);
        unsigned char* epmat = (unsigned char*) malloc (epmat_size);
        //unsigned  char epmat[64000];
        if (epmat == NULL) { return eeperr_Malloc; }
        memset(epmat, 0, epmat_size);

        ret = eepif.mem_read(addr_out[i].hwaddr, epmat, epmat_size);
        if (ret < 0) return ret;

        out = epmat2nmat(addr_out[i].shape[1], addr_out[i].shape[2], addr_out[i].shape[3], epmat, addr_out[i].exp);
        outputs.push_back(out);
        free(epmat);
    }

    return 0;
}



