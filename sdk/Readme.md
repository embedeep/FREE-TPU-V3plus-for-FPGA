## EEP-TPU SDK

- eeptpu_compiler
    EEP-TPU神经网络编译器，可编译不同框架的神经网络，例如：caffe、darknet、pytorch(onnx)、ncnn、keras等。
    编译后生成的bin文件可使用我们提供的API函数加载并在EEP-TPU上进行推理。
  
- libeeptpu_pub
    EEP-TPU的API函数接口。可加载EEP-TPU编译器生成的bin文件并进行推理。
    包含ARM32、AARCH64、X86三个平台的预编译好的库文件。
    运行demo示例程序前，需现将其解压缩并拷贝到demo/libs/目录下（具体请查看demo下的readme文档）。
  
- demo
    使用EEP-TPU的一些示例程序，例如分类网络示例、目标检测网络示例、ICNET语义分割网络示例等。
    里面有文档对这些示例程序进行介绍。
    demo里面所用到的原始模型文件和预编译好的bin文件，可从网盘里自行下载"sdk_demo_models"文件夹。
    网盘链接: https://pan.baidu.com/s/1P9ssGmT7KF4oaHVb-BWBMw 提取码: gup7 

- standalone
    net_model:包含yolov4-tiny神经网络模型及其编译脚本；
    src：裸机参考程序。

