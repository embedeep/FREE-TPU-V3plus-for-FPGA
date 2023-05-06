#include <stdio.h>
#include <vector>
#include <sys/time.h>
#include <algorithm>
#include <functional>
#include "eeptpu.h"
#include "npy_load.h"
#include "eep_image.h"

using namespace std;

static int get_topk(struct EEPTPU_RESULT& result , int topk, std::vector< std::pair<float, int> >& top_list);
static double get_current_time();

static EEPTPU *tpu = NULL;

static int eep_interface_type = eepInterfaceType_SOC;   // default.
//static int eep_interface_type = eepInterfaceType_PCIE;

static void results_release(vector<struct EEPTPU_RESULT>& results)
{
    for(unsigned int i = 0; i < results.size(); i++)
    {
        free(results[i].data);
    }
    results.clear();
}

static int eeptpu_init(int interface_type, char* path_bin)
{
    int ret; 
    
    if (tpu == NULL) tpu = tpu->init();
    
    if (interface_type == eepInterfaceType_PCIE) 
    {
        printf("Interface type: PCIE \n"); 
        ret = tpu->eeptpu_set_interface_info_pcie("/dev/xdma0_user", "/dev/xdma0_h2c_0", "/dev/xdma0_c2h_0");
        if (ret < 0) return ret;
            
        tpu->eeptpu_set_tpu_mem_base_addr(0x00000000);
        
        vector<struct EEPTPU_REG_ZONE> regzones;
        struct EEPTPU_REG_ZONE zone;
        zone.core_id = 0;   zone.addr = 0x00040000;     zone.size = 256*1024;     regzones.push_back(zone);
        tpu->eeptpu_set_tpu_reg_zones(regzones);
        
        ret = tpu->eeptpu_set_base_address(0x0,0x0,0x0,0x0);
        if (ret < 0) return ret;
    }
    else if (interface_type == eepInterfaceType_SOC) 
    {
        // use soc interface.
        vector<struct EEPTPU_REG_ZONE> regzones;
        struct EEPTPU_REG_ZONE zone;

      #if 1     // arm64
        zone.core_id = 0;   zone.addr = 0xA0000000;     zone.size = 0x1000;     regzones.push_back(zone);
        //zone.core_id = 1;   zone.addr = 0xA0040000;     zone.size = 0x1000;     regzones.push_back(zone); 
        tpu->eeptpu_set_tpu_reg_zones(regzones);
        
        // If need to set base address, use this, and change base address to your own.
        ret = tpu->eeptpu_set_base_address(0x40000000,0x40000000,0x40000000,0x40000000);
        if (ret < 0) return ret;

      #else     // arm32
        zone.core_id = 0;   zone.addr = 0x43C00000;     zone.size = 0x1000;     regzones.push_back(zone);
        tpu->eeptpu_set_tpu_reg_zones(regzones);
        membase = 0x30000000;  // arm32
        tpu->eeptpu_set_base_address(membase,membase,membase,membase);
      #endif
    }
    
    ret = tpu->eeptpu_set_interface(interface_type);
    if (ret < 0) return ret;

    printf("EEPTPU library  version: %s\n", tpu->eeptpu_get_lib_version());
    printf("EEPTPU hardware version: %s\n", tpu->eeptpu_get_tpu_version());
    printf("EEPTPU hardware info   : %s\n", tpu->eeptpu_get_tpu_info());
    
    ret = tpu->eeptpu_load_bin(path_bin);
    if (ret < 0) {
        printf("Load bin fail, ret=%d\n", ret);
        return ret;
    }

    printf("EEPTPU init ok\n");
    
    return 0;
}

static image_bytes eeptpu_write_input(char* path_image)
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
    if (argc != 3)
    {
        printf("Usage: %s eeptpu_bin_path image_path\n", argv[0]);
        return -1;
    }
    
    char *path_bin = argv[1];
    char *path_image = argv[2];
    int ret;
    double st, et;
    
    printf("\n# Embedeep EEPTPU Library Example - Classify #\n\n");

    ret = eeptpu_init(eep_interface_type, path_bin);
    if (ret < 0)
    {
        printf("EEPTPU init fail. [%d]\n", ret);
        return ret;
    }

    printf("Network input shape: [%d,%d,%d,%d]\n", tpu->input_shape[0], tpu->input_shape[1], tpu->input_shape[2], tpu->input_shape[3]);
    
    // ====== eeptpu init ok, only init one time in program.
    
    // ====== loop write input and forward if you have many images.
    
    image_bytes img_orig;
    img_orig = eeptpu_write_input(path_image);
    if (eepimg_empty(img_orig) == true)
    {
        printf("EEPTPU write input fail\n");
        return ret;
    }
    eepimg_free(img_orig);
    
    st = get_current_time();
    std::vector< std::pair<float, int> > top_list;

    std::vector<struct EEPTPU_RESULT> result;
    ret = tpu->eeptpu_forward(result);
    if (ret < 0) return ret;
        
    et = get_current_time();
    printf("EEPTPU forward ok, cost time(hw+sw): %.3f ms\n", et-st);
    unsigned int hwus = tpu->eeptpu_get_tpu_forward_time();
    printf("EEPTPU hw cost: %.3f ms\n", (float)hwus/1000);
    
    int topk = 5;
    if (result[0].shape[1] * result[0].shape[2] * result[0].shape[3] < topk) 
        topk = result[0].shape[1] * result[0].shape[2] * result[0].shape[3];
    ret = get_topk(result[0], topk, top_list);
    if (ret < 0) return ret;
        
    printf("Result (top %d): \n", topk);
    for (int i = 0; i < topk; i++)
    {
        printf("  [%3d] %.3f\n", top_list[i].second, top_list[i].first);
    }

    top_list.clear();
    results_release(result);
        
    // all images done, close eeptpu.
    if (tpu != NULL) { tpu->eeptpu_close();  delete(tpu); }
    
    printf("\nAll done\n");
    
    return 0;
}



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
    //if (result.h != 1 || result.w != 1) return -1;
    cls_scores.resize(result.shape[0]*result.shape[1]*result.shape[2]*result.shape[3]);
    
    int c = 0;
    for (int i = 0; i < result.shape[0]*result.shape[1]*result.shape[2]*result.shape[3]; i++)
    {
        cls_scores[c++] = result.data[i];
    }

    return get_topk(cls_scores, topk, top_list);
}

static double get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}


