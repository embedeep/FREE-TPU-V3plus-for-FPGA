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

#ifndef SRC_EEPTPU_EEPTPU_SA_H_
#define SRC_EEPTPU_EEPTPU_SA_H_


#include "../simplestl.h"
#include "interface/eep_interface.h"
#include "nmat.h"

using namespace std;

struct st_hwaddr_info
{
    unsigned long hwaddr;
    unsigned int shape[4];
    int exp;
};

class EEPTPU_SA
{
public:
    EEPTPU_SA();
    ~EEPTPU_SA();

    int mem_load(char* mem_path);
    int mem_load_from_ram(unsigned int addr, unsigned char* data, int datalen);

    int eeptpu_init(char* mem_path, char* ini_path);
    int eeptpu_init(unsigned char* data, int datalen, unsigned char* config, int cfglen);
    int eeptpu_deinit();
    int eeptpu_input(unsigned char* input_data, int datalen);

    int eepreg_read(unsigned int regaddr, unsigned int* regval);
    int eepreg_write(unsigned int regaddr, unsigned int regval);
    int eepreg_wait(unsigned long regaddr, unsigned int mask, unsigned int want_val);
    int tpu_read_hw_ver(unsigned int* regval);
    int eep_tpu_read_hw_config(unsigned int* regval);
    int eep_tpu_start_work(unsigned long addr);
    int eep_tpu_wait_done();

    int forward();
    int read_forward_result(vector< ncnn::Mat >& outputs);

public:
    //int tpu_hwbase_count;
    //unsigned int* tpu_hwbase;

    unsigned char membuf[1024*1024];
    unsigned int memhwaddr_start;

    // interface
    EEP_INTERFACE eepif;

    bool b_terminate;

    int interface;
    unsigned long mem_base;
    unsigned long hwbase0;
    unsigned long hwbase1;
    unsigned long memsize;
    unsigned long tpureg_addr;
    unsigned long reg_size;
    bool b_ex;

    // out addr
    vector<struct st_hwaddr_info> addr_out;
    // algorithm addr
    unsigned long addr_alg;
    struct st_hwaddr_info addr_in;
    vector<float> mean;
    vector<float> norm;

    int bin_type;
    int mem_cnt;
    unsigned long hwbase2;
    unsigned long hwbase3;
};



// Error Code
#define succ                        (0)
#define eeperr_Fail                 (-1)
#define eeperr_Param                (-2)
#define eeperr_NotSupport           (-3)
#define eeperr_FileOpen             (-4)
#define eeperr_FileRead             (-5)
#define eeperr_FileWrite            (-6)
#define eeperr_Malloc               (-7)
#define eeperr_Timeout              (-8)

#define eeperr_LoadBin              (-11)
#define eeperr_TpuInit              (-12)
#define eeperr_ImgType              (-13)
#define eeperr_PixelType            (-14)
#define eeperr_Mean                 (-15)

#define eeperr_BlobSrc              (-16)
#define eeperr_BlobFmt              (-17)
#define eeperr_BlobSize             (-18)
#define eeperr_BlobData             (-19)
#define eeperr_BlobOutput           (-20)

#define eeperr_MemAddr              (-21)
#define eeperr_MemWr                (-22)
#define eeperr_MemRd                (-23)
#define eeperr_MemMap               (-24)

#define eeperr_DevOpen              (-25)
#define eeperr_DevNotInit           (-26)
#define eeperr_InterfaceType        (-27)
#define eeperr_InterfaceInit        (-28)
#define eeperr_InterfaceOperate     (-29)

#define eeperr_EEPBinTooOld         (-40)
#define eeperr_EEPLibTooOld         (-41)
#define eeperr_TpuVerTooOld         (-42)


#endif /* SRC_EEPTPU_EEPTPU_SA_H_ */
