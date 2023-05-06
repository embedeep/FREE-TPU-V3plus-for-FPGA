## EEP-TPU SDK

- eeptpu_compiler
    EEP-TPU Compiler，support AI Framework，e.g. caffe、darknet、pytorch(onnx)、ncnn、keras。
    generating bin files be used by EEP-TPU API for AI inferance。
  
- libeeptpu_pub
    EEP-TPU API Interface。User can do AI inferance by loading eeptpu_compiler bins。
    support ARM32、AARCH64 and X86。
    refer to demo/readme for more details。
  
- demo
    The demos of EEP-TPU AI inferance, e.g. classify, object detection, semantic segmentation, and so on。
    User can downlaod prebuild bin from Baidu network disk:
      addr: https://pan.baidu.com/s/1P9ssGmT7KF4oaHVb-BWBMw ; passwd : gup7 

- standalone
    net_model:yolov4-tiny model and scripts for compile；
    src：standalone C++ code。

