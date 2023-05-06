#!/bin/bash

platform=$1
if [ ! -n "$platform" ]; then 
    #echo -e "Usage: " $0 " cbuf"
    echo -e "platform(arm32/aarch64/x86) not set, please choose platform: 32 or 64 or 86:"
    read -t 30 -p "Enter your select:  " sel
    
    if [ ! -n "$sel" ]; then 
        echo "No input, please check."
        exit 0
    fi

    platform=$sel
fi

if [ $platform == "32" ]; then
    CPP="arm-linux-gnueabihf-g++"
    pf="arm32"
elif [ $platform == "64" ]; then
    CPP="aarch64-linux-gnu-g++"
    pf="aarch64"
elif [ $platform == "86" ]; then
    CPP="g++"
    pf="x86"
fi

basepath=$(cd `dirname $0`; pwd)
basepath=${basepath}"/"
cd ${basepath}

input_files="./main.cpp 
             ../common/eepimg_v0.2.6/eep_image.cpp 
            "
cflags="-I./ -I../libs/${pf}/eep/include -I../common/eepimg_v0.2.6/"
ldflags="-L../libs/${pf}/eep/lib "
libs="-leeptpu_pub"
target="demo"
#cflags+=" -std=c++11 "

cmd="${CPP} -o ${target} -Wall -O3 ${input_files} ${cflags} ${ldflags} ${libs} -Wl,-rpath,../libs/${pf}/eep/lib -fopenmp"

echo "Compiling..."
echo ${cmd}
eval $cmd

if [ $? == 0 ]; then 
    echo -e "\n-->> Compile succ \n"
else
    echo -e "\n-->> Compile fail \n"
fi

cd - > /dev/null

