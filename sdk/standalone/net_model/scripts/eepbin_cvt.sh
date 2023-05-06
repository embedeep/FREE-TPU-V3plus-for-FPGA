 #!/bin/bash

cmd="../eepBinCvt --bin ./scripts/binRoot/yolov4tiny/eeptpu_s2.pub.bin --input ../models/images/ssd/004545.bmp --output header"

echo ${cmd}
eval $cmd

cmd="../eepBinCvt --bin ./scripts/binRoot/yolov4tiny/eeptpu_s2.pub.bin --input ../models/images/ssd/004545.bmp --output mem"

echo ${cmd}
eval $cmd

cd - > /dev/null
