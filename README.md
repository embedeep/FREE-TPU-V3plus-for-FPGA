# FREE TPU V3+ for FPGA
***UPDATE @ 2023.05***<br>
***Free TPU***  is the free version of a commercial AI processor (EEP-TPU) for Deep Learning EDGE Inference, which was first released four years ago.
After four years R&D, EEP-TPU has evolved into the second-generation architecture with V3+ version, and has been embedded in three ASIC chips to achieve mass production.<br>
(**The next-version release will support Transformer, star and watch this repository if you are interested**)

### The key feature of V3+ EEP-TPU
+ High Efficiency Computing
	* Reconfigurable architecture
	* Only Take Once data usage scheme
	* Low memory footprint optimization technology
+ Low Latency Computing
	* Dataflow architecture
	* Mixing-precision computing
	* Near memory computing technology
	* Multi-core with Multithreading technology
+ General Purpose Computing
	* Supporting CNN, LSTM and RNN algorithm 
	* Supporting FP16, FP8 and INT8 precision
	* Hundreds of native operators with operator extension
	* Typical algorithms including Whole Series YOLO, ICNet, MobileNet, Shufflenet, Resnet, LSTM...
	* Compile tools for Pytorch, Caffe, Darknet and NCNN framework

NOW, Free-TPU V3+, the free version of EEP-TPU V3+, is released.<br>
**Free TPU V3+ is same as EEP-TPU V3+ except that a one-hour halt limitation is set in Free TPU V3+ to limit commercial applications.**

### The Free-TPU V3+ IP package, including:
+ An encrypted IP with specific configuration(supporting FP16 and INT8 precision). 
+ A open-source demo design for ZynqMP FPGA with Linux OS
+ A open-source demo design for ZynqMP FPGA without Linux OS (Bare Metal)
+ A open-source SDK 
+ A Compiler for Pytorch, Caffe, Darknet and NCNN framework

***User also can download compressed git-project from baidu Netdisk link: https://pan.baidu.com/s/1-2hEask121Wpk3I3E_YdyQ  passwd: fkdn , and the passwd of compressed file is Embedeep***

### Performance of Free-TPU V3+
The performance of Free-TPU V3+ is highly dependent on the configuration, IP frequency and DDR memory bandwidth. We show the latency of a single-core ASIC design with 1GHZ frequency (2TFLOPS/4TOPS) as follow:

Algorithm|input size|precision|latency
:---:|:---:|:---:|:---:
MobileNetV2-1.0|224x224x3|FP16|2.01 ms
MobileNetV2-1.0|224x224x3|INT8|1.5 ms
Resnet50|224x224x3|FP16|9.52 ms
Resnet50|224x224x3|INT8|6.19 ms
Yolov5s|640x640x3|FP16|21.82 ms
Yolov5s|640x640x3|INT8|14.19 ms

+ To further improve the latency performance, the Multi-core with Multithreading technology is a good choice. For example, the INT8 latency of MobileNetV2-1.0 is 0.56 ms in an eight-core ASIC design. 

More details, please visit https://www.embedeep.com

### Run steps
Please refer to the document.

### Directory Structure
Directory Name | Note
:---:|:---:
constr|FPGA pin constraints file
doc|Documents of how to use Free TPU FPGA IP
hardware|xsa file, prebuild BOOTbin
ip_repo|The encrypted FPGA IPs
script|scripts of create vivado project of TPU IP
sdk|standalone and linux demos to use TPU IP  


## Contact
Questions can email us or be left as issues in the repository, we will be happy to answer them.
## Contributors
Luo (luohy@embedeep.com) <br>
Zhou (zhouzx@embedeep.com) <br>
He (herh@embedeep.com)
