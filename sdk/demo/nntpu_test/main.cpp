#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <string.h>
#include <csignal>
#include <iostream>
#include <fstream> 
#include <dirent.h>     // only for linux
#include <sys/stat.h>  
#include <algorithm>

#include "eeptpu.h"
#include "npy_load.h"
#include "eep_image.h"

using namespace std;

/*
v1.2: support image input.
v1.3: eeptpu_set_input, npy, mode=1: add data_type.
v1.4: support 4 channels image input.
v1.5: support npy input data to process mean/norm.
v1.6: add param '--txt_col <n>', can set column when saving data to file.
v2.0: support libeeptpu_pub.so v0.7.0 (support multi-inputs); 
      no need to set pack_shape if using pack mode(bin file must use compiler-v2.4.1 or newer to generate).
*/
string verstr = "v2.0";

static EEPTPU *tpu = NULL;

struct st_appargs
{
    string path_bin;
    string path_input;
    int pack_output_c;
    int pack_output_h;
    int pack_output_w;
    int interface;      // 1: soc; 2: pcie
    int txt_col;
    
    st_appargs():
        path_bin(""),
        path_input(""),
        pack_output_c(0),
        pack_output_h(0),
        pack_output_w(0),
        interface(1),
        txt_col(4)
    { }
};
static struct st_appargs appargs;

// one net maybe have multi-inputs.
struct st_input
{
    int id;
    string path;
};
// If network input number is 1, and will test N images: input_list[0][0], input_list[1][0], ..., input_list[N-1][0]
// If network input number is 3, and will test N times: (need N*3 images)
//      input_list[0][0], input_list[0][1], input_list[0][2],
//      input_list[1][0], input_list[1][1], input_list[1][2],
//      ......
//      input_list[N-1][0], input_list[N-1][1], input_list[N-1][2],
vector< vector<struct st_input> > input_list; 

void signal_handler( int sig );
static int getArgs(int argc, char* argv[]);
static void results_release(vector<struct EEPTPU_RESULT>& results);
static int eeptpu_write_input(int input_id, string path_input, NET_INPUT_INFO* input_info);
static int eeptpu_write_inputs(vector<struct st_input>& inputs, vector<struct NET_INPUT_INFO>& inputs_info);
static int get_topk(struct EEPTPU_RESULT& result , int topk, std::vector< std::pair<float, int> >& top_list);
static void print_top5(struct EEPTPU_RESULT& result);
static int input_parpare(string input_path_or_folder, vector< vector<struct st_input> >& in_list);
static void print_result(struct EEPTPU_RESULT& result);
static std::vector<std::string> split(std::string str,std::string pattern);
template <typename T> static bool is_vector_all_value(vector<T>& vec, T val);
template <typename T> static void print_mean_norm(vector<T>& vec, T default_val);

#include <sys/time.h>
double get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}


int main(int argc, char* argv[])
{
    printf("EEP-TPU test. [%s]\n", verstr.c_str());
    
    int ret;
    ret = getArgs(argc, argv);
    if (ret < 0) return ret;
    
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);

    printf("\n---------------- Init TPU ----------------\n");
    printf("Initializing TPU...\n");
    if (tpu == NULL) tpu = tpu->init();
    
    unsigned int membase;
    if (appargs.interface == 1)
    {
        // use soc interface.
        vector<struct EEPTPU_REG_ZONE> regzones;
        struct EEPTPU_REG_ZONE zone;
      #if 1     // arm64
        zone.core_id = 0;   zone.addr = 0xA0000000;     zone.size = 0x1000;     regzones.push_back(zone);
        //zone.core_id = 1;   zone.addr = 0xA0040000;     zone.size = 0x1000;     regzones.push_back(zone);
        tpu->eeptpu_set_tpu_reg_zones(regzones);
        membase = 0x40000000;  // arm64
        tpu->eeptpu_set_base_address(membase,membase,membase,membase);
      #else     // arm32
        zone.core_id = 0;   zone.addr = 0x43C00000;     zone.size = 0x1000;     regzones.push_back(zone);
        tpu->eeptpu_set_tpu_reg_zones(regzones);
        membase = 0x30000000;  // arm32
        tpu->eeptpu_set_base_address(membase,membase,membase,membase);
      #endif
        
        ret = tpu->eeptpu_set_interface(eepInterfaceType_SOC);
        if (ret < 0) return ret;
    }
    else if (appargs.interface == 2)
    {
        printf("Interface type: PCIE \n"); 
        ret = tpu->eeptpu_set_interface_info_pcie("/dev/xdma0_user", "/dev/xdma0_h2c_0", "/dev/xdma0_c2h_0");
        if (ret < 0) return ret;
            
        tpu->eeptpu_set_tpu_mem_base_addr(0x00000000);
        
        vector<struct EEPTPU_REG_ZONE> regzones;
        struct EEPTPU_REG_ZONE zone;
        zone.core_id = 0;   zone.addr = 0x00040000;     zone.size = 256*1024;     regzones.push_back(zone);
        tpu->eeptpu_set_tpu_reg_zones(regzones);
    
        ret = tpu->eeptpu_set_base_address(0x10000000,0x10000000,0x10000000,0x10000000);
        if (ret < 0) return ret;
            
        ret = tpu->eeptpu_set_interface(eepInterfaceType_PCIE);
        if (ret < 0) return ret;
    }
    else { printf("Unsupported interface %d.\n", appargs.interface); return -1; }
    
    // print version
    printf("EEPTPU library  version: %s\n", tpu->eeptpu_get_lib_version());
    printf("EEPTPU hardware version: %s\n", tpu->eeptpu_get_tpu_version());
    printf("EEPTPU hardware info   : %s\n", tpu->eeptpu_get_tpu_info());

    // load bin
    ret = tpu->eeptpu_load_bin(appargs.path_bin.c_str());
    if (ret < 0) {
        printf("Load bin fail, ret=%d\n", ret);
        return ret;
    }
    
    printf("EEPTPU init ok.\n");    

    std::vector<struct NET_INPUT_INFO> inputs_info;
    tpu->eeptpu_get_input_info(inputs_info);
    printf("Network input info: \n");
    for (int i = 0; i < (int)inputs_info.size(); i++)
    {
        printf("    InputID[%d] shape: [%d,%d,%d]; name: %s; ", inputs_info[i].input_id, inputs_info[i].c, inputs_info[i].h, inputs_info[i].w, inputs_info[i].name.c_str());
        if (inputs_info[i].pack_type > 0) 
        { 
            printf("pack_type: %d; pack_shape: %d,%d,%d; ", inputs_info[i].pack_type, inputs_info[i].pack_out_c, inputs_info[i].pack_out_h, inputs_info[i].pack_out_w);
        }
        printf("\n");
    }

    // appargs.pack_output_c/h/w: only for 1 input. 
    // here for bin file which generated by compiler earlier than 2.4.1 (pack shape not saved in bin file)
    // if bin file generated by compiler earlier than 2.4.1 and using pack mode, need munual set the pack shape by '--pack_shape'
    if ( (inputs_info[0].pack_out_c == 0) || (inputs_info[0].pack_out_h == 0) || (inputs_info[0].pack_out_w == 0) )
    {
        if (appargs.pack_output_c != 0 && appargs.pack_output_h != 0 && appargs.pack_output_w != 0)
        {
            inputs_info[0].pack_out_c = appargs.pack_output_c;
            inputs_info[0].pack_out_h = appargs.pack_output_h;
            inputs_info[0].pack_out_w = appargs.pack_output_w;
            printf("User sets pack shape to: %d,%d,%d\n", inputs_info[0].pack_out_c, inputs_info[0].pack_out_h, inputs_info[0].pack_out_w);
        }
    }
    else  
    {
        if (appargs.pack_output_c != 0 && appargs.pack_output_h != 0 && appargs.pack_output_w != 0)
        {
            printf("Not used '--pack_shape', because we use pack shape from bin file.\n");
        }
    }
    
    struct NET_MEM_SIZE meminfo = tpu->eeptpu_get_memory_zone_size();

    printf("\n---------------- Find inputs ----------------\n");
    input_parpare(appargs.path_input, input_list);
    // Normally one network only have 1 input(1 group have 1 input); but some networks have multi-inputs(1 group have N inputs)
    printf("The network's input count=%d; User set %d group of input data to test.\n", (int)input_list[0].size(), (int)input_list.size());

    int idx = 0;
    while (idx < (int)input_list.size())
    {
        printf("\n----------------------------- Write input %d ------------------------------\n", idx);
        //printf("Processing input data and write to TPU...\n");
        ret = eeptpu_write_inputs(input_list[idx], inputs_info);
        if (ret < 0) return ret;
        //printf("Write input data ok.\n");
    
        //printf("\n---------------- TPU forward ----------------\n");
        std::vector<struct EEPTPU_RESULT> result;
        double st = get_current_time();
        ret = tpu->eeptpu_forward(result);
        double et = get_current_time();
        if (ret < 0) return ret;
        printf("EEPTPU forward ok, cost time(hw+sw): %.3fms\n", et - st);
        unsigned int hwus = tpu->eeptpu_get_tpu_forward_time();
        printf("EEPTPU hardware cost: %.3f ms\n", (float)hwus/1000);
        
        //printf("\n---------------- Results ----------------\n");
        for (int i = 0; i < (int)result.size(); i++)
        {
            printf("--->>> result[%d]: %d,%d,%d\n", i, result[i].shape[1], result[i].shape[2], result[i].shape[3]);
            //print_result(result[i]);
        }
        if (result.size() == 1) print_top5(result[0]);
            
#if 1
        if (1)
        {
            for (int m = 0; m < (int)result.size(); m++)
            {
                float* p = (float*)result[m].data;
                FILE* fpin;
                char fname[256];
                if (m==0) sprintf(fname, "%s", (char*)"test_output_data.txt");
                else sprintf(fname, "test_output_data_%d.txt", m);
                
                fpin = fopen(fname, "wb");
                int cnt = 0;
                for(int c = 0; c < result[m].shape[1] * result[m].shape[2] * result[m].shape[3]; c++)
                {
                    fprintf(fpin, "%f,", *p++);
                    //fprintf(fpin, "%d,", (signed char)(*p++));
                    cnt++;
                    if (cnt == appargs.txt_col) { fprintf(fpin, "\n"); cnt = 0; }
                }
                if (cnt > 0) { fprintf(fpin, "\n"); cnt = 0; }
                fclose(fpin);
            }
        }
#endif
        
        results_release(result);
        
        idx ++;
    }
    
    printf("\n---------------- Release resources ----------------\n");
    printf("Closing TPU...\n");
    tpu->eeptpu_close(); 
    printf("TPU closed!\n");
    
    //printf("\n-------------------------------\n");
    delete(tpu);
    tpu = NULL;
    
    printf("All done\n\n");

    return 0;
}


void signal_handler( int sig )
{
    if ( (sig == SIGINT) || (sig == SIGTSTP)  || ( sig == SIGSEGV) || ( sig == SIGFPE ))
    {
        printf("\n> Will exit program, please wait... Got signal(%d)\n", sig);
        if (tpu != NULL) { tpu->eeptpu_close(); delete(tpu); tpu = NULL; }
        exit(EXIT_FAILURE);
    }
}


static void usage(char* exe)
{
    printf("Usage: \n");
    printf("    %s [options]\n", exe);
    printf("    options:\n");
    printf("        -h (--help)         # Show help.\n");
    printf("        --bin <path>        # Bin file path.\n");
    printf("        --input <path>      # Input path or folder.\n");
    printf("        --interface <n>     # Set interface. 1: SOC(default); 2: PCIE.");
    printf("        --txt_col <n>       # If save input/output data to txt file, this will set the data count in 1 line, default is 4.\n");
    printf("        --pack_shape <str>  # set pack shape, format: 'c,h,w'. (example: --pack_shape '3,256,256').\n");
    printf("                              --pack_shape: only for bin file which generated by compiler earlier than v2.4.1\n");
    printf("\n");
}

static int getArgs(int argc, char* argv[])
{
    bool b_help = false;
    int i = 1;
    while(true)
    {
        if (i >= argc) break;
        const char* arg = argv[i];    
        
        if (strcmp(arg, "-h") == 0) { b_help = true; break; }
        else if (strcmp(arg, "--help") == 0) { b_help = true; break; }
        else if (strcmp(arg, "--bin") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.path_bin = string(argv[++i]);
        }
        else if (strcmp(arg, "--input") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.path_input = string(argv[++i]);
        }
        else if (strcmp(arg, "--pack_shape") == 0)
        {
            if (i+1 >= argc) { break; }
            sscanf(argv[++i], "%d,%d,%d", &appargs.pack_output_c, &appargs.pack_output_h, &appargs.pack_output_w);
        }
        else if (strcmp(arg, "--interface") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.interface = atoi(argv[++i]);
        }
        else if (strcmp(arg, "--txt_col") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.txt_col = atoi(argv[++i]);
            if (appargs.txt_col < 0) appargs.txt_col = 4;
        }
        else { b_help = true; break; }
        i++;
    }
    if (b_help) { usage(argv[0]); return -1; }
    
    return 0;
}


static void print_result(struct EEPTPU_RESULT& result)
{
    printf("result shape: %d,%d,%d\n", result.shape[1], result.shape[2], result.shape[3]);
    int cnt = 0;
    float* p = result.data;
    for(int c = 0; c < result.shape[1] * result.shape[2] * result.shape[3]; c++)
    {
        printf("%f,", *p++);
        cnt++;
        if (cnt == 16) { printf("\n"); cnt = 0; }
    }
    if (cnt > 0) printf("\n");
}

static void results_release(vector<struct EEPTPU_RESULT>& results)
{
    for(unsigned int i = 0; i < results.size(); i++)
    {
        free(results[i].data);
    }
    results.clear();
}

template <typename T>
static void input_save_txt(int id, void* src_data, int srclen, const char* fmt)
{
    char fname[64] = {0};
    if (id == 0)
        sprintf(fname, "test_input_data.txt");
    else
        sprintf(fname, "test_input_data_%d.txt", id);
    T* p = (T*)src_data;
    FILE* fpin = fopen(fname, "wb");
    int cnt = 0;
    for(int c = 0; c < srclen; c++)
    {
        fprintf(fpin, fmt, *p++);
        cnt++;
        if (cnt == appargs.txt_col) { fprintf(fpin, "\n"); cnt = 0; }
    }
    if (cnt > 0) { fprintf(fpin, "\n"); cnt = 0; }
    fclose(fpin);
}

template <typename T>
static void process_mean_norm(float* pdst, void* src, int c, int h, int w, vector<float>& mean, vector<float>& norm)
{
    T* psrc = (T*)src;
    int hw = h*w;
    for (int k = 0; k < c; k++)    // channel
    {
        float* pd = (float*)pdst + k*hw;
        T* ps = (T*)psrc + k*hw;
        for (int j = 0; j < hw; j++) 
        {
            *pd++ = (*ps++ - mean[k]) * norm[k];
        }
    }
}


static int eeptpu_write_inputs(vector<struct st_input>& inputs, std::vector<struct NET_INPUT_INFO>& inputs_info)
{
    int ret = 0;
    for (int i = 0; i < (int)inputs.size(); i++)
    {
        int info_id = 0;
        for (int j = 0; j < (int)inputs_info.size(); j++)
        {
            if (inputs_info[j].input_id == inputs[i].id)
            {
                info_id = j;
                //printf("input_id=%d: find inputs_info[%d] input_id=%d, name=%s\n", inputs[i].id, j, inputs_info[j].input_id, inputs_info[j].name.c_str());
                break;
            }
        }
        ret = eeptpu_write_input(inputs[i].id, inputs[i].path, &inputs_info[info_id]);
        if (ret < 0) return ret;
    }
    return 0;
}
static int eeptpu_write_input(int input_id, string path_input, NET_INPUT_INFO* input_info)
{
    int ret = 0;
    
    printf("InputID[%d]: %s\n", input_id, (char*)path_input.c_str());

    std::string ext = path_input.substr(path_input.find_last_of(".") + 1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "npy")
    {
        printf("Use npy input\n");
        void* src_data = NULL;
        int shp[4] = {1,0,0,0}; // shape[0] always be 1.   b,c,h,w
        int data_type;
        ret = read_npy(path_input, &src_data, &(shp[1]), &(shp[2]), &(shp[3]), &data_type);
        if (ret < 0)
        {
            printf("npy data(%s) read fail.\n", path_input.c_str());
            return ret;
        }
        //printf("Read npy ok, shape:%d,%d,%d, data_type:%d\n", shp[1], shp[2], shp[3], data_type);
        
#if 1
        int srclen = shp[1] * shp[2] * shp[3];
        switch(data_type)
        {
            case 1: input_save_txt<float>(input_id, src_data, srclen, "%f,"); break;  // fp32
            case 2: input_save_txt<short>(input_id, src_data, srclen, "%f,"); break;  // fp16
            case 3: input_save_txt<signed char>(input_id, src_data, srclen, "%d,"); break;  // char
            case 4: input_save_txt<short>(input_id, src_data, srclen, "%d,"); break;  // short
            case 5: input_save_txt<int>(input_id, src_data, srclen, "%d,"); break;  // int
            case 7: input_save_txt<unsigned char>(input_id, src_data, srclen, "%d,"); break;  // uchar
            default : break;
        }
#endif
        
        void* in_data = src_data;
        bool b_in_data_need_free = false;

        // check network's mean/norm setting.
        vector<float> net_mean; vector<float> net_norm;
        tpu->eeptpu_get_mean_norm(net_mean, net_norm);
        if ( (is_vector_all_value<float>(net_mean, 0.0) == false) || (is_vector_all_value<float>(net_norm, 1.0) == false) )
        {
            printf("Warning: Input is npy file, and found 'mean,norm' in this network. \n");
            printf("         Make sure the input npy data is processed 'mean&norm'!\n");
            printf("         Mean: "); print_mean_norm<float>(net_mean, 0.0); printf("\n");
            printf("         Norm: "); print_mean_norm<float>(net_norm, 1.0); printf("\n");
            if (1)
            {
                vector<float> use_mean; vector<float> use_norm;
                use_mean.assign(net_mean.begin(), net_mean.end());
                use_norm.assign(net_norm.begin(), net_norm.end());
                if ( (int)use_mean.size() < shp[1] )    // maybe use 'del_last_channels' mode.
                {
                    int cnt = shp[1] - use_mean.size();
                    for (int k = 0; k < cnt; k++) 
                    {
                        use_mean.push_back(0.0);
                        use_norm.push_back(1.0);
                    }
                }

                in_data = (float*)malloc(shp[1]*shp[2]*shp[3]*sizeof(float));
                b_in_data_need_free = true;
                switch(data_type)
                {
                    case 1: process_mean_norm<float>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // fp32
                    case 2: process_mean_norm<short>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // fp16
                    case 3: process_mean_norm<signed char>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // char
                    case 4: process_mean_norm<short>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // short
                    case 5: process_mean_norm<int>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // int
                    case 7: process_mean_norm<unsigned char>((float*)in_data, src_data, shp[1], shp[2], shp[3], use_mean, use_norm); break;  // uchar
                    default : printf("Not supported data type.\n"); if (src_data != NULL) free(src_data); return -1; 
                }
                data_type = 1;  // float32
                use_mean.clear();
                use_norm.clear();
                printf("We processed 'mean&norm' for npy data.\n");
            }
        }

        if (input_info->pack_out_c == 0)
        {
            ret = tpu->eeptpu_set_input(input_id, src_data, 
                                        shp[1], shp[2], shp[3],
                                        1,
                                        data_type   // mode=1, npy input, need set data_type.
                                        );
        }
        else 
        {
            ret = tpu->eeptpu_set_input(input_id, src_data, 
                                        shp[1], shp[2], shp[3],
                                        2   // mode=2: set raw data.
                                        );
        }
        
        if (ret < 0) {
            printf("Set EEPTPU input fail, ret = %d\n", ret);
            if (src_data != NULL) free(src_data);
            return ret;
        }
        
        free(src_data);
        if (b_in_data_need_free) free(in_data);
    }
    else if (ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "png" || ext == "pgm")
    {
        image_bytes img;
        int use_c = input_info->c;
        int use_h = input_info->h;
        int use_w = input_info->w;
        
        if (input_info->pack_out_c > 0) 
        { 
            use_c = input_info->pack_out_c; 
            use_h = input_info->pack_out_h; 
            use_w = input_info->pack_out_w; 
        }
        
        if (use_c == 3)   // 3 channels
        {
            // input_shape: 1c,2h,3w
            // load image and resize to (w,h)
            img = eepimg_load_image((char*)path_input.c_str(), EEPIMG_PIXEL_BGR, use_w, use_h, 1.0);  
        }
        else if (use_c == 1)
        {
            img = eepimg_load_image((char*)path_input.c_str(), EEPIMG_PIXEL_GRAY, use_w, use_h, 1.0);
        }
        else if (use_c == 4)
        {
            img = eepimg_load_image((char*)path_input.c_str(), EEPIMG_PIXEL_BGRA, use_w, use_h, 1.0);
        }
        
        if (eepimg_empty(img) == true) {
            printf("Read image fail\n");
            return -1;
        }
        
        if (input_info->pack_out_c == 0)
        {
            if (use_c < 4)
            {
                ret = tpu->eeptpu_set_input(input_id, img.data, img.c, img.h, img.w, 0);
            }
            else if (use_c == 4)      // make sure the mean&norm is processed by tpu, OR not need to use mean&norm
            {
                image_bytes img_chw = eepimg_hwc2chw(img);
                float* fbuf = (float*)malloc(img_chw.c * img_chw.h * img_chw.w * sizeof(float));
                float* pdst = fbuf;
                unsigned char* psrc = img_chw.data;
                for (int k = 0; k < img.c * img.h * img.w; k++) *pdst++ = *psrc++;
                ret = tpu->eeptpu_set_input(fbuf, img_chw.c, img_chw.h, img_chw.w, 1);
                free(fbuf);
                eepimg_free(img_chw);
            }
        }
        else 
        {
            ret = tpu->eeptpu_set_input(input_id, img.data, use_c, use_h, use_w, 2);
        }
        
        eepimg_free(img);
        if (ret < 0) 
		{
            printf("Set EEPTPU input fail, ret = %d\n", ret);
            return ret;
        }
    }
    else
    {
        printf("Not supported file extension(%s) of input data.\n", ext.c_str());
        ret = -1;
    }
    
    return ret;
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
    cls_scores.resize(result.shape[0]*result.shape[1]*result.shape[2]*result.shape[3]);
    
    int c = 0;
    for (int i = 0; i < result.shape[0]*result.shape[1]*result.shape[2]*result.shape[3]; i++)
    {
        cls_scores[c++] = result.data[i];
    }

    return get_topk(cls_scores, topk, top_list);
}

static void print_top5(struct EEPTPU_RESULT& result)
{
    int ret = 0;
    std::vector< std::pair<float, int> > top_list;
    int topk = 5;
    if (result.shape[1] * result.shape[2] * result.shape[3] < topk) 
        topk = result.shape[1] * result.shape[2] * result.shape[3];
    ret = get_topk(result, topk, top_list);
    if (ret < 0) return;
    printf("TPU forward results (top %d): \n", topk);
    for (int i = 0; i < topk; i++)
    {
        printf("  [%d] %5d,   %.3f,  \n", i, top_list[i].second, top_list[i].first);
    }
    top_list.clear();
}

// if 1 input: just the path
// if multi-input: 'id1;path1#id2;path2#...'
static int input_parpare(string input_info, vector< vector<struct st_input> >& in_list)
{
    bool b_multi_input = false;
    // if the string have '#' and ';', then we treat it as multi-input.
    if ( (input_info.find("#") != string::npos) && (input_info.find(";") != string::npos) ) b_multi_input = true;

    if (!b_multi_input)
    {
        string file_path = input_info;
        struct stat s_buf;  
        stat(file_path.c_str(), &s_buf); 
        
        in_list.clear();

        if(S_ISDIR(s_buf.st_mode))  
        {  
            if (file_path.c_str()[file_path.length()-1] != '/') {
                file_path +=  "/";
            }

            int i=0;
            std::string absolute_path;
            std::string file_name;
            
            vector<struct st_input> items;

            struct dirent **namelist = NULL;
            int n;
            n = scandir(file_path.c_str(), &namelist, 0,  alphasort);
            for (int k = 0; k < n; k++)
            {
                //file_name = ptr->d_name;
                file_name = namelist[k]->d_name;
                std::string ext = file_name.substr(file_name.find_last_of(".") + 1);
                transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if ( ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "pgm" || ext == "bmp" || ext == "ppm"
                     || ext == "npy"
                   ) {
                    absolute_path = file_path;
                    absolute_path += file_name;
                    i++;
                    struct st_input item;
                    item.id = 0;
                    item.path = absolute_path;
                    items.push_back(item);
                }
            }
            if (namelist != NULL) free(namelist);
            in_list.push_back(items);
        }
        else {
            vector<struct st_input> items;
            struct st_input item;
            item.id = 0;
            item.path = file_path;
            items.push_back(item);
            in_list.push_back(items);
        }

        //printf("Find input count = %d\n", (int)in_list[0].size());
    }
    else 
    {
        vector<string> a = split(input_info, "#");
        vector<struct st_input> items;
        for (int i = 0; i < (int)a.size(); i++)
        {
            if (a[i].length() == 0) continue;
            vector<string> b = split(a[i], ";");
            for (int j = 0; j < (int)b.size(); j++)
            {
                if (b[j].length() == 0)
                {
                    b.erase(b.begin() + j);
                    j--;
                }
            }
            if (b.size() != 2)
            {
                printf("Multi-input string format not correct! (%s)\n", input_info.c_str());
                return -1;
            }
            
            struct st_input item;
            item.id = atoi(b[0].c_str());
            item.path = b[1];
            items.push_back(item);
        }
        in_list.push_back(items);

        //printf("Set multi-input group = %d, each group have %d inputs.\n", (int)in_list.size(), (int)in_list[0].size());
    }

    return 0;
}


// --- utils ---
static std::vector<std::string> split(std::string str,std::string pattern)  
{  
    std::string::size_type pos;  
    std::vector<std::string> result;  
      
    str+=pattern;
    int size=str.size();  
      
    for(int i=0; i<size; i++)  
    {  
        pos=str.find(pattern,i);  
        if((int)pos < size)  
        {  
            std::string s=str.substr(i,pos-i);  
            result.push_back(s);  
            i=pos+pattern.size()-1;  
        }  
    }  
    return result;  
}  

template <typename T>
static bool is_vector_all_value(vector<T>& vec, T val)
{
    for (unsigned int i = 0; i < vec.size(); i++)
    {
        if (vec[i] != val)
        {
            return false;
        }
    }
    return true;
}

template <typename T>
static void print_mean_norm(vector<T>& vec, T default_val)
{
    if (is_vector_all_value(vec, default_val))
    {
        printf("All %f.", (float)default_val);
        return;
    }

    for (unsigned int i = 0; i < vec.size(); i++)
    {
        printf("%f,", vec[i]);
    }
}

