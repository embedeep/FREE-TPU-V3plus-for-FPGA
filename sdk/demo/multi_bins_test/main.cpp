#include <stdio.h>
#include <vector>
#include <string.h>
#include <sys/time.h>
#include <algorithm>
#include <functional>
#include "eeptpu.h"
#include "eep_image.h"

using namespace std;

static EEPTPU *tpu_classify = NULL;
static EEPTPU *tpu_det = NULL;

struct Object
{
    int x;
    int y;
    int w;
    int h;
    int label;
    float prob;
    char obj_class_name[32];
};
static std::vector<Object> g_objects;
static char text[256];

static double get_current_time();
static int post_process_obj_detect(image_bytes img, struct EEPTPU_RESULT& result);
static image_bytes draw_objects(image_bytes, const std::vector<Object> &objects);
static void clear_objects();
static int objdet_forward_test(char* path_image);
static int classify_forward_test(char* path_image);
static int get_topk(const std::vector<float>& cls_scores, unsigned int topk, std::vector< std::pair<float, int> >& top_list);
static int get_topk(struct EEPTPU_RESULT& result , int topk, std::vector< std::pair<float, int> >& top_list);

static void results_release(vector<struct EEPTPU_RESULT>& results)
{
    for(unsigned int i = 0; i < results.size(); i++)
    {
        free(results[i].data);
    }
    results.clear();
}

#if defined(INTERFACE_PCIE)
    static int eep_interface_type = eepInterfaceType_PCIE;
#else
    static int eep_interface_type = eepInterfaceType_SOC;   // default.
#endif

static int eeptpu_init(EEPTPU*& tpu, int interface_type, char* path_bin, int fg_multi)
{
    int ret; 
    
    if (tpu == NULL) tpu = tpu->init();
    
#if defined(INTERFACE_PCIE)
    
    if (interface_type != eepInterfaceType_PCIE) return -1;
    printf("Interface type: PCIE \n"); 
    ret = tpu->eeptpu_set_interface_info_pcie("/dev/xdma0_user", "/dev/xdma0_h2c_0", "/dev/xdma0_c2h_0");
    if (ret < 0) return ret;
        
    tpu->eeptpu_set_tpu_mem_base_addr(0x00000000);
    
    vector<struct EEPTPU_REG_ZONE> regzones;
    struct EEPTPU_REG_ZONE zone;
    zone.core_id = 0;   zone.addr = 0x00040000;     zone.size = 256*1024;     regzones.push_back(zone);
    tpu->eeptpu_set_tpu_reg_zones(regzones);
    
    if (fg_multi == 0)
    {
        ret = tpu->eeptpu_set_base_address(0x0,0x0,0x0,0x0);
        if (ret < 0) return ret;
    }

#else 
    
    // use soc interface.
    vector<struct EEPTPU_REG_ZONE> regzones;
    struct EEPTPU_REG_ZONE zone;
  #if 1     // arm64
    zone.core_id = 0;   zone.addr = 0xA0000000;     zone.size = 0x1000;     regzones.push_back(zone);
    zone.core_id = 1;   zone.addr = 0xA0040000;     zone.size = 0x1000;     regzones.push_back(zone);
    tpu->eeptpu_set_tpu_reg_zones(regzones);
    if (fg_multi == 0)
    {
        // If need to set base address, use this, and change base address to your own.
        ret = tpu->eeptpu_set_base_address(0x40000000,0x40000000,0x40000000,0x40000000);
        if (ret < 0) return ret;
    }
  #else     // arm32
    zone.core_id = 0;   zone.addr = 0x43C00000;     zone.size = 0x1000;     regzones.push_back(zone);
    tpu->eeptpu_set_tpu_reg_zones(regzones);
    membase = 0x30000000;  // arm32
    tpu->eeptpu_set_base_address(membase,membase,membase,membase);
  #endif
      
#endif    
    
    ret = tpu->eeptpu_set_interface(interface_type);
    if (ret < 0) return ret;
    
    if (fg_multi == 0)
    {
        printf("EEPTPU library  version: %s\n", tpu->eeptpu_get_lib_version());
        printf("EEPTPU hardware version: %s\n", tpu->eeptpu_get_tpu_version());
        printf("EEPTPU hardware info   : %s\n", tpu->eeptpu_get_tpu_info());
    }
    
    printf("Loading %s\n", path_bin);
    ret = tpu->eeptpu_load_bin(path_bin, fg_multi);
    if (ret < 0) {
        printf("Load bin fail, ret=%d\n", ret);
        return ret;
    }

    printf("EEPTPU init ok\n");
    
    return 0;
}

static image_bytes eeptpu_write_input(EEPTPU* tpu, char* path_image)
{
    int ret;
    image_bytes img_orig;
    
    if (tpu->input_shape[1] == 3)
    {
        img_orig = eepimg_load_image(path_image, EEPIMG_PIXEL_BGR);
        //img_orig = eepimg_load_image(path_image, EEPIMG_PIXEL_RGB);  // if use darknet
    }
    else if (tpu->input_shape[1] == 1)
    {
        img_orig = eepimg_load_image(path_image, EEPIMG_PIXEL_GRAY);
    }
    if (eepimg_empty(img_orig) == true) {
        printf("Read image fail\n");
        return img_orig;
    }
    
    image_bytes img = eepimg_resize(img_orig, tpu->input_shape[3], tpu->input_shape[2]);
    
    ret = tpu->eeptpu_set_input(img.data, img.c, img.h, img.w, 0);
    eepimg_free(img);
    if (ret < 0) {
        printf("Set EEPTPU input fail, ret = %d\n", ret);
        eepimg_free(img_orig);
        return img_orig;
    }

    return img_orig;
}


int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        printf("Usage: \n");
        printf("       sudo %s bin_classify bin_yolo image_path\n", argv[0]);
        return -1;
    }
    
    char *path_bin_classify = argv[1];
    char *path_bin_detect = argv[2];
    char *path_image = argv[3];
    int ret;
    
    printf("\n# Embedeep EEPTPU Library Example - Multi-bins test #\n\n");
    printf("Example for: EEP-TPU 1 Core & 2 Bins\n\n");
    
    printf("Initing classify bin file...\n");
    ret = eeptpu_init(tpu_classify, eep_interface_type, path_bin_classify, 0);
    if (ret < 0)
    {
        printf("EEPTPU init fail.\n");
        return ret;
    }
    printf("Classify network input shape: [%d,%d,%d,%d]\n", tpu_classify->input_shape[0], tpu_classify->input_shape[1], tpu_classify->input_shape[2], tpu_classify->input_shape[3]);
    
    printf("\nIniting object detection bin file...\n");
    ret = eeptpu_init(tpu_det, eep_interface_type, path_bin_detect, 1);
    if (ret < 0)
    {
        printf("EEPTPU init fail.\n");
        return ret;
    }
    printf("Object detection network input shape: [%d,%d,%d,%d]\n", tpu_det->input_shape[0], tpu_det->input_shape[1], tpu_det->input_shape[2], tpu_det->input_shape[3]);

    // ====== eeptpu init ok, only init one time in program.
    

    ret = classify_forward_test(path_image);
    if (ret < 0) 
    {
        printf("classify_forward_test fail, will return, ret = %d\n", ret);
        return ret;
    }
    
    ret = objdet_forward_test(path_image);
    if (ret < 0) 
    {
        printf("objdet_forward_test fail, will return, ret = %d\n", ret);
        return ret;
    }

    // all images done, close eeptpu.
    if (tpu_classify != NULL) { tpu_classify->eeptpu_close();  delete(tpu_classify); }
    if (tpu_det != NULL) { tpu_det->eeptpu_close(); delete(tpu_det); }
    clear_objects();
    
    printf("\nAll done\n");
    
    return 0;
}

static int classify_forward_test(char* path_image)
{
    int ret = 0;
    printf("\n==== classify ====\n");
    
    image_bytes img_orig;
    img_orig = eeptpu_write_input(tpu_classify, path_image);
    if (eepimg_empty(img_orig) == true)
    {
        printf("EEPTPU write input fail\n");
        return ret;
    }
    eepimg_free(img_orig);
    
    std::vector<struct EEPTPU_RESULT> result;
    double st = get_current_time();
    ret = tpu_classify->eeptpu_forward(result);
    if (ret < 0) 
    {
        printf("eeptpu forward fail, ret = %d\n", ret);
        return ret;
    }   
    double et = get_current_time();
    printf("EEPTPU forward ok, cost time(hw+sw): %.3f ms\n", et-st);        // hardware + software
    unsigned int hwus = tpu_classify->eeptpu_get_tpu_forward_time();
    printf("EEPTPU hardware cost: %.3f ms\n", (float)hwus/1000);
    
    printf("result shape: %d,%d,%d\n", result[0].shape[1], result[0].shape[2], result[0].shape[3]);
    std::vector< std::pair<float, int> > top_list;
    ret = get_topk(result[0], 5, top_list);
    if (ret < 0) { printf("get top fail\n"); return ret; }
    
    printf("Result (top 5): \n");
    for (int i = 0; i < 5; i++)
    {
        printf("  [%3d] %.3f\n", top_list[i].second, top_list[i].first);
    }
    
    results_release(result);
    top_list.clear();
    
    return 0;
}

static int objdet_forward_test(char* path_image)
{
    int ret = 0;
    
    printf("\n==== object detection ====\n");
    image_bytes img_orig;
    img_orig = eeptpu_write_input(tpu_det, path_image);
    if (eepimg_empty(img_orig) == true)
    {
        printf("EEPTPU write input fail\n");
        return ret;
    }
    
    std::vector<struct EEPTPU_RESULT> result;
    double st = get_current_time();
    
    ret = tpu_det->eeptpu_forward(result);
    if (ret < 0) 
    {
        printf("eeptpu forward fail, ret = %d\n", ret);
        return ret;
    }   
    
    double et = get_current_time();
    printf("EEPTPU forward ok, cost time(hw+sw): %.3f ms\n", et-st);    // hardware + software
    unsigned int hwus = tpu_det->eeptpu_get_tpu_forward_time();
    printf("EEPTPU hardware cost: %.3f ms\n", (float)hwus/1000);
    
    if (result.size() == 0)
    {
        printf("Nothing detected. \n");
    }
    else
    {
        ret = post_process_obj_detect(img_orig, result[0]);
        if (ret < 0) return ret;

        // draw object
        image_bytes final_image = draw_objects(img_orig, g_objects);
        
        eepimg_save("./objdet.jpg", final_image);       // yolo object detect result save to image.
        //printf("Saved result image to: ./objdet.jpg\n");
        
        if (eepimg_empty(final_image) == false) eepimg_free(final_image);        
    }
    
    results_release(result);
    if (eepimg_empty(img_orig) == false) eepimg_free(img_orig);  
    
    return 0;
}

static double get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}


// ====== object detect process ======

#if 0
static const char *class_names_default[] = {"background",
                                    "aeroplane", "bicycle", "bird", "boat",
                                    "bottle", "bus", "car", "cat", "chair",
                                    "cow", "diningtable", "dog", "horse",
                                    "motorbike", "person", "pottedplant",
                                    "sheep", "sofa", "train", "tvmonitor"};
#else
static const char *class_names_default[] = {
    "background","person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed","diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
};
#endif

char** class_names = (char **)class_names_default;

static void clear_objects()
{
    g_objects.clear();
}

static int post_process_obj_detect(image_bytes img, struct EEPTPU_RESULT& result)
{
    clear_objects();
    
    if ( (result.shape[1] * result.shape[2] * result.shape[3] == 0) || (result.shape[1] != 1) )
    {
        printf("Nothing detected !\n");
        return 0;
    }

    for (int i = 0; i < result.shape[2]; i++)
    {
        const float *values = result.data + i * result.shape[3];

        Object object;
        object.label = values[0];
        object.prob = values[1];
        object.x = values[2] * img.w;
        object.y = values[3] * img.h;
        object.w = values[4] * img.w - object.x;
        object.h = values[5] * img.h - object.y;
        strcpy(object.obj_class_name, class_names[object.label]);
        
        if (object.x < 0) { object.w += object.x; object.x = 0; }
        if (object.y < 0) { object.h += object.y; object.y = 0; }

        g_objects.push_back(object);
    }
   
    printf("Detection result: \n");
    for (unsigned int i = 0; i < g_objects.size(); i++)
    {
        const Object &obj = g_objects[i];
        printf("    [%d] %d = %.5f at %d %d %d x %d   (%s)\n", i, obj.label, obj.prob,
                obj.x, obj.y, obj.w, obj.h, class_names[obj.label]);
    }
        
    return 0;
}

static image_bytes draw_objects(image_bytes img, const std::vector<Object> &objects)
{    
    image_bytes image = eepimg_copy_image(img);

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object &obj = objects[i];
        image = eepimg_draw_box(image, 
                                obj.x, obj.y, 
                                obj.x+obj.w, obj.y+obj.h,
                                0, 255, 0, 1);
        
        sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);
        int x = obj.x;
        int y = obj.y;
        image = eepimg_draw_text(image, x, y, text);
    }

    return image;
}


// ================ classify top k ===================
static int get_topk(const std::vector<float>& cls_scores, unsigned int topk, std::vector< std::pair<float, int> >& top_list)
{
    if (cls_scores.size() < topk) topk = cls_scores.size();

    int size = cls_scores.size();
    std::vector< std::pair<float, int> > vec;
    vec.resize(size);
    for (int i=0; i<size; i++)
    {
        vec[i] = std::make_pair(cls_scores[i], i);
    }

    std::partial_sort(vec.begin(), vec.begin() + topk, vec.end(),
                      std::greater< std::pair<float, int> >());
    
    top_list.resize(topk);
    for (unsigned int i=0; i<topk; i++)
    {
        top_list[i].first = vec[i].first;
        top_list[i].second = vec[i].second;
    }

    return 0;
}

static int get_topk(struct EEPTPU_RESULT& result , int topk, std::vector< std::pair<float, int> >& top_list)
{
    std::vector<float> cls_scores;
    cls_scores.resize(result.shape[1]*result.shape[2]*result.shape[3]);
    
    int c = 0;
    for (int i = 0; i < result.shape[1]*result.shape[2]*result.shape[3]; i++)
    {
        cls_scores[c++] = result.data[i];
    }

    return get_topk(cls_scores, topk, top_list);
}

