#include <stdio.h>
#include <vector>
#include <sys/time.h>
#include <fstream> 
#include <iostream> 
#include <algorithm>
#include <map>
#include <dirent.h>     // only for linux
#include <sys/stat.h>  
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "eeptpu.h"
#include "eep_image.h"

using namespace std;

static double get_current_time();
static int input_parpare(string input_path_or_folder, vector<string>& in_list);

static EEPTPU *tpu = NULL;

// Semantic segmentation ( Neural network: ICNET )
static unsigned char seg_color[19][3] = 
{
    { 0,  200, 150 },       // road,        green
    { 244,  0, 232 },       // sidewalk,    purple
    {  70,  70,  70 }, 
    { 102, 102, 156 }, 
    { 190, 153, 153 }, 
    { 153, 153, 153 }, 
    { 250, 170,  30 }, 
    { 220, 220,   0 }, 
    { 107, 142,  35 }, 
    { 152, 251, 152 }, 
    {  70, 130, 180 }, 
    { 220,  20,  60 },      // person,      red
    { 255,   0,   0 }, 
    {   0,   0, 142 },      // car,         blue
    {   0,   0,  70 }, 
    {   0,  60, 100 }, 
    {   0,  80, 100 }, 
    {   0,   0, 230 }, 
    { 119,  11,  32 },  
};

// --- app arguments ---
struct st_appargs
{
    string input;
    string path_bin;
    int interface;
    
    st_appargs():
        input(""),
        path_bin(""),
        interface(eepInterfaceType_SOC)
    { }
};
static struct st_appargs appargs;

#include <csignal>
void signal_handler( int sig )
{
    if ( ( sig == SIGINT) || ( sig == SIGTSTP ) || ( sig == SIGSEGV) || ( sig == SIGFPE ))
    {
        printf("\n> Pressed exit or exception... Will exit program, please wait...\n");
        if (tpu != NULL) { tpu->eeptpu_close(); delete(tpu); }
        exit(EXIT_FAILURE);
    }
}

static int eeptpu_init()
{
    int ret = 0;
    
    printf("EEP-TPU config:\n");
    printf("    bin: %s\n", appargs.path_bin.c_str());
    printf("    interface: %d\n", appargs.interface);

    if (tpu == NULL) tpu = tpu->init();
    
    if (appargs.interface == eepInterfaceType_PCIE)
    {
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
    else if (appargs.interface == eepInterfaceType_SOC) 
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

    ret = tpu->eeptpu_set_interface(appargs.interface);
    if (ret < 0) return ret;
    
    printf("EEP-TPU library  version: %s\n", tpu->eeptpu_get_lib_version());
    printf("EEP-TPU hardware version: %s\n", tpu->eeptpu_get_tpu_version());
    printf("EEP-TPU hardware info   : %s\n", tpu->eeptpu_get_tpu_info());
    
    ret = tpu->eeptpu_load_bin(appargs.path_bin.c_str());
    if (ret < 0) {
        printf("Load bin fail, ret=%d\n", ret);
        return ret;
    }
    
    printf("EEP-TPU init ok\n");    
    return 0;
}

static double get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void results_release(vector<struct EEPTPU_RESULT>& results)
{
    for(unsigned int i = 0; i < results.size(); i++)
    {
        free(results[i].data);
    }
    results.clear();
}

// ---------- neural network ----------
void usage(char* exe)
{
    printf("Usage: \n");
    printf("    %s [options]\n", exe);
    printf("    options:\n");
    printf("        --help(-h)          # show help.\n");
    printf("        --bin               # eeptpu bin file.\n");
    printf("        --input             # input image path or folder.\n");
    printf("        --iface             # interface. 1-soc; 2-pcie\n");
    printf("        --baseaddr <b0;b1>  # base address. format: \"base0;base1\"\n");
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
            appargs.input = string(argv[++i]);
        }
        else if (strcmp(arg, "--iface") == 0) 
        { 
            if (i+1 >= argc) { break; }
            int interface = atoi(argv[++i]);
            if (interface == 1) appargs.interface = eepInterfaceType_SOC;
            else if (interface == 2) appargs.interface = eepInterfaceType_PCIE;
        }
        else 
        {
            printf("Unknown: %s\n", arg);
            b_help = true; break; 
        }
        i++;
    }
    if (b_help) { usage(argv[0]); return -1; }
    
    if (appargs.path_bin == "") { printf("Please set bin path. (use -h for help)\n"); return -1; }
    if (appargs.input == "") { printf("Please set input. (use -h for help)\n"); return -1; }
    printf("input: %s\n", appargs.input.c_str());
    
    return 0;
}

int main(int argc, char* argv[])
{
    int ret; 
    ret = getArgs(argc, argv);
    if (ret < 0) return ret;
    
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGSEGV, signal_handler);    // segment fault
    signal(SIGFPE, signal_handler);
    
    char *path_image = (char*)appargs.input.c_str();
    printf("\n# EEP-TPU Example - ICNET #\n\n");

    vector<string> input_list;
    input_parpare(path_image, input_list);

    ret = eeptpu_init();
    if (ret < 0)
    {
        printf("EEPTPU init fail.\n");
        return ret;
    }
    printf("Network input shape: [%d,%d,%d,%d]\n", tpu->input_shape[0], tpu->input_shape[1], tpu->input_shape[2], tpu->input_shape[3]);

    // ====== init ok, only init one time in program.
    
    // ====== loop write input and forward if you have many images.
    int imgidx = 0;
    do 
    {
        image_bytes img_orig;
        image_bytes img_resized;
        
        // read image
        printf("\n[%d/%d] load img: %s\n", imgidx+1, (int)input_list.size(), (char*)input_list[imgidx].c_str());

        if (tpu->input_shape[1] == 3)
        {
            img_orig = eepimg_load_image((char*)input_list[imgidx].c_str(), EEPIMG_PIXEL_BGR);
        }
        else if (tpu->input_shape[1] == 1)
        {
            img_orig = eepimg_load_image((char*)input_list[imgidx].c_str(), EEPIMG_PIXEL_GRAY);
        }
        if (eepimg_empty(img_orig) == true) {
            printf("Read image fail\n");
            return -1;
        }
        
        img_resized = eepimg_resize(img_orig, tpu->input_shape[3], tpu->input_shape[2]);

        // set image data to memory
        ret = tpu->eeptpu_set_input(img_resized.data, img_resized.c, img_resized.h, img_resized.w, 0);
        if (ret < 0) 
        {
            printf("Set EEPTPU input fail\n");
            return ret;
        }
        printf("Set EEPTPU input ok\n");
        
        // eeptpu forward
        printf("Forwarding\n");
        std::vector<struct EEPTPU_RESULT> results;
        double st = get_current_time();
        ret = tpu->eeptpu_forward(results);
        if (ret < 0) 
        {
            printf("eeptpu forward fail, ret = %d\n", ret);
            return ret;
        }   
        double et = get_current_time();
        printf("forward ok, cost time: %.3f ms\n", et-st);    
	    float hwms = tpu->eeptpu_get_tpu_forward_time() / 1000.0;
	    printf("hardware cost: %.3f ms\n", hwms);
        
        // post process
        if (results.size() > 0)
        {
            int readval;
            printf("result dims: chw %d,%d,%d\n", results[0].shape[1], results[0].shape[2], results[0].shape[3]);

            // set img1 from tpu result
            image_bytes img_seg = eepimg_make_image_bytes(results[0].shape[3], results[0].shape[2], 3);
            unsigned char* pseg = img_seg.data;
            // im_seg: rgbrgbrgb....
            {
                float* p_out = (float*)results[0].data;
                int offset = 0;
                for (int h = 0; h < results[0].shape[2]; h++)
                {
                    for (int w = 0; w < results[0].shape[3]; w++)
                    {
                        readval = (int)(*p_out++);
                        /*  Neural network: ICNET
                        0-road          1-sidewalk      2-building      3-wall
                        4-fence         5-pole          6-traffic light 7-traffic sign
                        8-vegetation    9-terrain       10-sky          11-person
                        12-rider        13-car          14-truck        15-bus
                        16-train        17-motorcycle   18-bicycle
                        */
                        *(pseg + offset + 0) = seg_color[readval][2];
                        *(pseg + offset + 1) = seg_color[readval][1];
                        *(pseg + offset + 2) = seg_color[readval][0];
                        offset += 3;
                    }
                }
            }
            
            image_bytes img_final = eepimg_copy_image(img_resized);
            for (int i = 0; i < img_final.c * img_final.h * img_final.w; i++)
            {
                img_final.data[i] = img_final.data[i] * 0.4 + img_seg.data[i] * 0.6;
            }

            eepimg_save("./icnet_result.jpg", img_final);
            printf("Saved result image to: ./icnet_result.jpg\n");
            
            eepimg_free(img_final);
            eepimg_free(img_seg);
        }

        eepimg_free(img_orig);
        eepimg_free(img_resized);
        results_release(results);
        
        imgidx++;
        if (imgidx >= (int)input_list.size()) break;
    }while(1);
    
    tpu->eeptpu_close();
    printf("\nAll done\n");
    
    return 0;
}


////////////////////////////////////////////////////////////////////
//////// --- common ---

static int input_parpare(string input_path_or_folder, vector<string>& in_list)
{
    struct stat s_buf;  
    stat(input_path_or_folder.c_str(), &s_buf); 
    
    in_list.clear();
    
    if(S_ISDIR(s_buf.st_mode))  
    {  
        if (input_path_or_folder.c_str()[input_path_or_folder.length()-1] != '/') {
            input_path_or_folder +=  "/";
        }

        int i=0;
        std::string absolute_path;
        std::string file_name;
        
        struct dirent **namelist = NULL;
        int n;
        n = scandir(input_path_or_folder.c_str(), &namelist, 0,  alphasort);
        for (int k = 0; k < n; k++)
        {
            //file_name = ptr->d_name;
            file_name = namelist[k]->d_name;
            std::string ext = file_name.substr(file_name.find_last_of(".") + 1);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if ( ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "pgm" || ext == "bmp" || ext == "ppm") 
            {
                absolute_path = input_path_or_folder;
                absolute_path += file_name;
                i++;
                in_list.push_back(absolute_path);
            }
        }
        if (namelist != NULL) free(namelist);
    }
    else {
        in_list.push_back(input_path_or_folder);
    }

    printf("Find input count = %d\n", (int)in_list.size());
    return 0;
}
