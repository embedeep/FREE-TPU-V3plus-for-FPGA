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

#include <stdlib.h>

void* operator new(size_t sz) noexcept
{
    void* ptr = malloc(sz);
    return ptr;
}

void* operator new(size_t sz, void* ptr) noexcept
{
    return ptr;
}

void* operator new[](size_t sz) noexcept
{
    void* ptr = malloc(sz);
    return ptr;
}

void* operator new[](size_t sz, void* ptr) noexcept
{
    return ptr;
}

void operator delete(void *ptr) noexcept
{
    free(ptr);
}

void operator delete(void *ptr, size_t sz) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr, size_t sz) noexcept
{
    free(ptr);
}




