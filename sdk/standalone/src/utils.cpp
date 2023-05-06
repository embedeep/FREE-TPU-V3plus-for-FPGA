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

static bool AllIsNum(char* str)
{
    int slen = strlen(str);
    for (int i = 0; i < slen; i++)
    {
        int tmp = (int)str[i];
        if (tmp >= 48 && tmp <= 57)
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

int str2num(char* str, unsigned int* val)
{
    if (AllIsNum(str) == true)
    {
        sscanf(str, "%d", val);
    }
    else if ((strlen(str) > 2) && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        sscanf(str, "%x", val);      // hex
    }
    else
        return -1;
    return 0;
}




