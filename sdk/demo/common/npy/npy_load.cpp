#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <map>

#include "cnpy.h"

#define  DAT_FORMAT_FP32    1
#define  DAT_FORMAT_FP16    2
#define  DAT_FORMAT_INT8    3
#define  DAT_FORMAT_INT16   4
#define  DAT_FORMAT_INT32   5
#define  DAT_FORMAT_UINT8   7     // uint 8

// tpu not support uint16/uint32
//#define  DAT_FORMAT_UINT16  8     // uint 16
//#define  DAT_FORMAT_UINT32  9     // uint 32

int read_npy(std::string filepath, void** buff, int* shape1, int* shape2, int* shape3, int* dtype)
{
    *shape1 = 0; *shape2 = 0; *shape3 = 0;
    
    //load it into a new array
    //printf("loading npy\n");
    cnpy::NpyArray arr = cnpy::npy_load(filepath);
    //float* loaded_data = (float*)arr.data<float>();
    
#if 0
    printf("--------------------\n");
    printf("npy size: %ld\n", arr.num_bytes());
    printf("shape: ");
    for (unsigned int i = 0; i < arr.shape.size(); i++)
    {
        printf("%ld,", arr.shape[i]);
    }
    printf("\n");
    printf("npy word_size: %d\n", arr.word_size);
    printf("npy element_type: %s\n", arr.element_type.c_str());
    printf("npy fortran_order: %d\n", arr.fortran_order);
    printf("--------------------\n");
#endif

    if (arr.fortran_order == 1)
    {
        printf("[Warning] npy data is fortran order, you must manual convert the data's order!\n");
        return -1;
    }

    int ib = 1;
    int s1,s2,s3; s1=s2=s3=0;
    if (arr.shape.size() == 4)
    {
        ib = arr.shape[0];
        s1 = arr.shape[1];
        s2 = arr.shape[2];
        s3 = arr.shape[3];
    }
    else if (arr.shape.size() == 3)
    {
        ib = 1;
        s1 = arr.shape[0];
        s2 = arr.shape[1];
        s3 = arr.shape[2];
    }
    else if (arr.shape.size() == 2)
    {
        ib = s1 = 1;
        s2 = arr.shape[0];
        s3 = arr.shape[1];
    }
    else if (arr.shape.size() == 1)
    {
        ib = s1 = s2 = 1;
        s3 = arr.shape[0];
    }
    else 
    {
        printf("[Error] Not supported npy shape.\n");
        return -1;
    }

    // element_type maybe: b1, i1, i2, i4, i8, u1, u2, u4, u8, f4, f8, c8, c16
    if (arr.element_type == "f4")
    {
        //printf("loading float32\n");
        *dtype = DAT_FORMAT_FP32;
        *buff = (float *)malloc( ib*s1*s2*s3 * sizeof(float));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        float* ptrdst = (float*)*buff;
        
        float* npy_data = (float*)arr.data<float>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else if (arr.element_type == "f2")
    {
        //printf("loading float16\n");
        *dtype = DAT_FORMAT_FP16;
        *buff = (short *)malloc( ib*s1*s2*s3 * sizeof(short));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        short* ptrdst = (short*)*buff;
        
        short* npy_data = (short*)arr.data<short>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else if (arr.element_type == "i4")
    {
        *dtype = DAT_FORMAT_INT32;
        *buff = (int *)malloc( ib*s1*s2*s3 * sizeof(int));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        int* ptrdst = (int*)*buff;
        
        int* npy_data = (int*)arr.data<int>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else if (arr.element_type == "i2")
    {
        //printf("loading int16\n");
        *dtype = DAT_FORMAT_INT16;
        *buff = (short *)malloc( ib*s1*s2*s3 * sizeof(short));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        short* ptrdst = (short*)*buff;
        
        short* npy_data = (short*)arr.data<short>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else if (arr.element_type == "u1")
    {
        *dtype = DAT_FORMAT_UINT8;
        *buff = (unsigned char *)malloc( ib*s1*s2*s3 * sizeof(unsigned char));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        unsigned char* ptrdst = (unsigned char*)*buff;
        
        unsigned char* npy_data = (unsigned char*)arr.data<unsigned char>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else if (arr.element_type == "i1")
    {
        *dtype = DAT_FORMAT_INT8;
        *buff = (char *)malloc( ib*s1*s2*s3 * sizeof(char));
        if(! *buff) 
        {
            perror("malloc error");
            return -1;
        }
    
        char* ptrdst = (char*)*buff;
        
        char* npy_data = (char*)arr.data<char>();
        for (int i = 0; i < ib*s1*s2*s3; i++) *ptrdst++ = *npy_data++;
    }
    else
    {
        printf("Not supported npy element type (%s) yet. \n", arr.element_type.c_str());
        return -1;
    }
    *shape1 = s1;
    *shape2 = s2;
    *shape3 = s3;
    
    return 0;
}