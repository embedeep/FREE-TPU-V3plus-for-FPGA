#!/bin/bash
basepath=$(cd `dirname $0`; pwd)
basepath=${basepath}"/"
cd ${basepath}

# setting.ini
# !! Must set compiler/model_root/global_cmd/bin_name in setting.ini
#
# Usage example: 
#     bash b_mnet1.sh netName 
# or: bash b_mnet1.sh netName binDir
# or: bash b_mnet1.sh netName binDir simDir
#

compiler=""
model_root=""
global_cmd="" 

# bin file will move to ${binDir}/${netName}/xx.bin 
# sim data will move to ${simdir}/${netName}/ 
netName="yolov4tiny"
binDir="binRoot"
simDir=


###########  ini  ############################
function read_ini() {
    file=$1;section=$2;item=$3;
    val=$(awk -F '=' '/\['${section}'\]/{a=1} (a==1 && "'${item}'"==$1){a=0;print $2}' ${file}) 
    echo ${val}
}

compiler=$(read_ini "./setting.ini" "Setting" "compiler")
echo -e "compiler=$compiler"
if [ ! -n "$compiler" ]; then echo "compiler not set, please set in file: setting.ini->Setting->compiler"; exit 0; fi 

model_root=$(read_ini "./setting.ini" "Setting" "model_root")
echo -e "model_root=$model_root"
if [ ! -n "$model_root" ]; then echo "model_root not set, please set in file: setting.ini->Setting->model_root"; exit 0; fi 

global_cmd=$(read_ini "./setting.ini" "Setting" "global_cmd")
echo -e "global_cmd=$global_cmd"

bin_name=$(read_ini "./setting.ini" "Setting" "bin_name")
echo -e "bin_name=$bin_name"

if [ $# -ge 1 ] ; then
    netName=$1
fi

if [ $# -ge 2 ] ; then
    binDir=$2
fi

if [ $# -ge 3 ] ; then
    simDir=$3
fi 

echo -e "netName=${netName}"
echo -e "binDir=${binDir}"
echo -e "simDir=${simDir}"

if [ -n "${binDir}" ]; then
    if [ ! -d ${binDir} ]; then
        mkdir ${binDir}
    fi
    
    if [ ! -d ${binDir}/${netName} ]; then
        mkdir ${binDir}/${netName}
    else
        rm -rf ${binDir}/${netName}
        mkdir ${binDir}/${netName}
    fi
fi 

if [ -n "${simDir}" ]; then
    if [ ! -d ${simDir} ]; then
        mkdir ${simDir}
    fi
    
    if [ ! -d ${simDir}/${netName} ]; then
        mkdir ${simDir}/${netName}
    else
        rm -rf ${simDir}/${netName}
        mkdir ${simDir}/${netName}
    fi
fi 


cfg=${model_root}"yolov4tiny/yolov4_tiny.cfg" 
wts=${model_root}"yolov4tiny/yolov4_tiny.weights"
img_ssd=${model_root}"/images/004545.bmp"
input_dir="--input_folder "${model_root}"images/ssd/"

extinfo=" --extinfo 'classes=background,person,bicycle,car,motorbike,aeroplane,bus,train,truck,boat,traffic light,fire hydrant,stop sign,parking meter,bench,bird,cat,dog,horse,sheep,cow,elephant,bear,zebra,giraffe,backpack,umbrella,handbag,tie,suitcase,frisbee,skis,snowboard,sports ball,kite,baseball bat,baseball glove,skateboard,surfboard,tennis racket,bottle,wine glass,cup,fork,knife,spoon,bowl,banana,apple,sandwich,orange,broccoli,carrot,hot dog,pizza,donut,cake,chair,sofa,pottedplant,bed,diningtable,toilet,tvmonitor,laptop,mouse,remote,keyboard,cell phone,microwave,oven,toaster,sink,refrigerator,book,clock,vase,scissors,teddy bear,hair drier,toothbrush' "

cmd="${compiler} ${global_cmd} --output ./ --mean '0.0,0.0,0.0' --norm '0.003921569,0.003921569,0.003921569' --darknet_cfg ${cfg} --darknet_weight ${wts} --image ${img_ssd} ${extinfo} ${input_dir}"

echo ${cmd}
eval $cmd

if [ $? == 0 ]; then 
    if [ -d ${simDir}/${netName} ]; then
        if [ -d ./output ] && [ `ls -A ./output |wc -w` -gt 0 ]; then
            echo -e "move output data to ${simDir}/${netName}"
            mv ./output/* ${simDir}/${netName}
        fi
    fi 
    if [ -d ${binDir}/${netName} ]; then
        echo -e "move bin file(${bin_name}) to ${binDir}/${netName}"
        mv ./${bin_name} ${binDir}/${netName}
    fi
    
    echo -e "Compile done.\n"
else
    echo -e "Compile fail.\n" 
    cd - > /dev/null
    exit -1
fi

cd - > /dev/null
