#ifndef _NPY_LOAD_H
#define _NPY_LOAD_H

#include <string>

int read_npy(std::string filepath, void** buff, int* shape1, int* shape2, int* shape3, int* dtype);
    
#endif
