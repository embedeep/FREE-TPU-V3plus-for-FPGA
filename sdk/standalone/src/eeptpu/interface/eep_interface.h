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

#ifndef SRC_EEPTPU_INTERFACE_EEP_INTERFACE_H_
#define SRC_EEPTPU_INTERFACE_EEP_INTERFACE_H_




class EEP_INTERFACE
{
public:
    EEP_INTERFACE();
    ~EEP_INTERFACE();

    // global functions
    int mem_write(unsigned long addr, unsigned long loadlen, unsigned char *datbuf, unsigned long buf_size);
    int mem_read(unsigned long addr, unsigned char* readbuf, unsigned long readlen);
    int register_write(unsigned long addr, unsigned int wr_val);
    int register_read(unsigned long addr, unsigned int* rd_val);
    int register_wait(unsigned long addr, unsigned int mask, unsigned int want_val);


public:
    unsigned long tpu_reg_base;      // for ddr&pcie. tpu registers base address.  ddr: tpu hwaddr;  pcie: tpu hwaddr offset.
    unsigned long mem_base_addr;
};

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)

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


#endif /* SRC_EEPTPU_INTERFACE_EEP_INTERFACE_H_ */
