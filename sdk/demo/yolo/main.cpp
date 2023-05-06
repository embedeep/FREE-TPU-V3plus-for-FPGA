#include <stdio.h>
#include <vector>
#include <sys/time.h>
#include <string.h>
#include "eeptpu.h"
#include "eep_image.h"

using namespace std;

static double get_current_time();

static EEPTPU *tpu = NULL;

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

static int post_process_obj_detect(image_bytes img, struct EEPTPU_RESULT& result);
static image_bytes draw_objects(image_bytes img, const std::vector<Object> &objects);
static void clear_objects();

static void print_result_data(struct EEPTPU_RESULT& result);
static int save_result(char* path_res, vector<struct EEPTPU_RESULT>& results);
std::vector<std::string> split(std::string str,std::string pattern);
string& trim(string &str);

// --- app arguments ---
struct st_appargs
{
    string bin_path;    
    string input;
    string pixel_order;
    
    st_appargs():
        bin_path(""),
        input(""),
        pixel_order("BGR")
    { }
};
static struct st_appargs appargs;

#include <csignal>
void signal_handler( int sig )
{
    if ( (sig == SIGINT) || (sig == SIGTSTP)  || ( sig == SIGSEGV) || ( sig == SIGFPE ))
    {
        printf("\n> Will exit program, please wait... Got signal(%d)\n", sig);
        if (tpu != NULL) { tpu->eeptpu_close(); delete(tpu); tpu = NULL; }
        exit(EXIT_FAILURE);
    }
}

static vector<string> classnames;
static void get_class_names(string name_list, vector<string>& classlist)
{
    vector<string> list = split(name_list, ",");
    classlist.clear();
    for (unsigned int i = 0; i < list.size(); i++)
    {
        classlist.push_back(trim(list[i]));
    }
}
static char s_class_name[64];
static char* get_class_name_by_label(int label)
{
    if ((int)classnames.size() > label)
        sprintf(s_class_name, "%s", classnames[label].c_str());        
    else 
        sprintf(s_class_name, "[C%d]", label);
    return s_class_name;
}

#if defined(INTERFACE_PCIE)
    static int eep_interface_type = eepInterfaceType_PCIE;
#else
    static int eep_interface_type = eepInterfaceType_SOC;   // default.
#endif

static int eeptpu_init(int interface_type, char* path_bin)
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
    
    ret = tpu->eeptpu_set_base_address(0x0,0x0,0x0,0x0);
    if (ret < 0) return ret;
    
#else
    
    vector<struct EEPTPU_REG_ZONE> regzones;
    struct EEPTPU_REG_ZONE zone;
    zone.core_id = 0;   zone.addr = 0xA0000000;     zone.size = 0x1000;     regzones.push_back(zone);
    //zone.core_id = 1;   zone.addr = 0xA0040000;     zone.size = 0x1000;     regzones.push_back(zone);
    tpu->eeptpu_set_tpu_reg_zones(regzones);

    tpu->eeptpu_set_base_address(0x40000000,0x40000000,0x40000000,0x40000000);
    
#endif

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
    
    // check whether the bin file contains the ssd's class names.
    char* extinfo = tpu->eeptpu_get_extinfo();
    if (strlen(extinfo) > 0)
    {
        string info = string(extinfo);
        if (info.find(";") == string::npos) info += ";";     // maybe not have ';', such as 'classes=xx,xx,xx'
        vector<string> list = split(info, ";");
        for (unsigned int i = 0; i < list.size(); i++)
        {
            if (list[i].find("classes") != string::npos) 
            {
                get_class_names(list[i].substr(8), classnames);
            }
        }
        if (classnames.size() > 0)
        {
            printf("Class names: ");
            for (unsigned int i = 0; i < classnames.size(); i++) { printf("%s,", classnames[i].c_str()); }
            printf("\n");
        }
    }
    
    printf("EEPTPU init ok\n");    
    return 0;
}

static image_bytes eeptpu_write_input(char* path_image, int in_c, int in_h, int in_w, int pixel_order)
{
    int ret;
    image_bytes img_orig;
    
    if (tpu->input_shape[1] == 3)
    {
        img_orig = eepimg_load_image(path_image, pixel_order);
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

static double get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// ====== object detect process ======

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
        strcpy(object.obj_class_name, get_class_name_by_label(object.label));
        
        if (object.x < 0) { object.w += object.x; object.x = 0; }
        if (object.y < 0) { object.h += object.y; object.y = 0; }

        g_objects.push_back(object);
    }
   
    printf("Detection result: \n");
    for (unsigned int i = 0; i < g_objects.size(); i++)
    {
        const Object &obj = g_objects[i];
        printf("    [%d] %d = %.5f at %d %d %d x %d   (%s)\n", i, obj.label, obj.prob,
                obj.x, obj.y, obj.w, obj.h, get_class_name_by_label(obj.label));
    }
        
    return 0;
}

static image_bytes draw_objects(image_bytes img, const std::vector<Object> &objects)
{    
    image_bytes image = eepimg_copy_image(img);

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object &obj = objects[i];
        
        int x = obj.x;
        int y = obj.y;
        sprintf(text, "%s %.1f%%", get_class_name_by_label(obj.label), obj.prob * 100);
        image = eepimg_draw_text(image, x, y, text);
        
        image = eepimg_draw_box(image, 
                                obj.x, obj.y, 
                                obj.x+obj.w, obj.y+obj.h,
                                0, 255, 0, 1);
    }

    return image;
}

static void print_result_data(struct EEPTPU_RESULT& result)
{
    float* pf = result.data;
    for (int c = 0; c < result.shape[1]; c++)
    {
        printf("C[%-2d]: ", c);
        for (int h = 0; h < result.shape[2]; h++)
        {
            for (int w = 0; w < result.shape[3]; w++)
            {
                printf("%.4f,", *pf++);
            }
            printf(" | ");
        }
        printf("\n");
    }
}
static int save_result(char* path_res, vector<struct EEPTPU_RESULT>& results)
{
    FILE* fd = fopen(path_res, "wb");
    
    fprintf(fd, "Neural network output count: %d \r\n", (int)results.size());
    for (unsigned int i = 0; i < results.size(); i++)
    {
        fprintf(fd, "Neural network output[%d] shape: 1,%d,%d,%d \r\n", i, results[i].shape[1], results[i].shape[2], results[i].shape[3]);
    }
    
    for (unsigned int i = 0; i < results.size(); i++)
    {
        fprintf(fd, "\noutput[%d]: \n", i);
        float* ptrf = (float*)results[i].data;
        for (int c = 0; c < results[i].shape[1]; c++)
        {
            for (int h = 0; h < results[i].shape[2]; h++)
            {
                fprintf(fd, "[0,%d,%d,x]: \n", c, h);
                int cnt = 0;
                for (int w = 0; w < results[i].shape[3]; w++)
                {
                    fprintf(fd, "%f, ", *ptrf++);
                    cnt++;
                    if (cnt == 16) { fprintf(fd, "\n"); cnt = 0; }
                }
                fprintf(fd, "\n");
            }
            fprintf(fd, "\n");
        }
    }
    
    fclose(fd);
    
    return 0;
}

// --- utils ---
std::vector<std::string> split(std::string str,std::string pattern)  
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

#include <cctype>
#include <iostream>
#include <algorithm>
// string trim 
inline string& ltrim(string &str) 
{
    string::iterator p = find_if(str.begin(), str.end(), not1(ptr_fun<int, int>(isspace)));
    str.erase(str.begin(), p);
    return str;
} 
inline string& rtrim(string &str) 
{
    string::reverse_iterator p = find_if(str.rbegin(), str.rend(), not1(ptr_fun<int , int>(isspace)));
    str.erase(p.base(), str.end());
    return str;
} 
string& trim(string &str) 
{
    ltrim(rtrim(str));
    return str;
}

void usage(char* exe)
{
    printf("%s version 1.0\n", exe);
    printf("Usage: \n");
    printf("    %s [options]\n", exe);
    printf("    options:\n");
    printf("        -h        # show help.\n");
    printf("        --bin     # eeptpu bin file path.\n");
    printf("        --input   # input image path.\n");
    printf("        --pixel   # pixel order, default is 'BGR'. Only valid for color input.\n"); 
    printf("                    If bin file converted from darknet, maybe need to set to 'RGB'. \n");
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
        else if (strcmp(arg, "--bin") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.bin_path = string(argv[++i]);
        }
        else if (strcmp(arg, "--input") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.input = string(argv[++i]);
        }
        else if (strcmp(arg, "--pixel") == 0) 
        { 
            if (i+1 >= argc) { break; }
            appargs.pixel_order = string(argv[++i]);
        }
        else { b_help = true; break; }
        i++;
    }
    if (b_help) { usage(argv[0]); return -1; }
    
    if (appargs.bin_path == "") { printf("Please set eeptpu bin path. (use -h for help)\n"); return -1; }
    if (appargs.input == "") { printf("Please set input. (use -h for help)\n"); return -1; }
    if (appargs.pixel_order != "RGB" && appargs.pixel_order != "BGR") { printf("Pixel order error, please check. (use -h for help)\n"); return -1; }
    
    return 0;
}

int main(int argc, char* argv[])
{
    int ret; 
    ret = getArgs(argc, argv);
    if (ret < 0) return ret;
    
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
    
    char *path_bin = (char*)appargs.bin_path.c_str();
    char *path_image = (char*)appargs.input.c_str();
    int pixel_order = EEPIMG_PIXEL_BGR;
    if (appargs.pixel_order == "RGB") pixel_order = EEPIMG_PIXEL_RGB;
    else if (appargs.pixel_order == "BGR") pixel_order = EEPIMG_PIXEL_BGR;
    
    printf("\n# Embedeep EEPTPU Library Example - Object Detection #\n\n");

    ret = eeptpu_init(eep_interface_type, path_bin);
    if (ret < 0)
    {
        printf("EEPTPU init fail.\n");
        return ret;
    }

    printf("Network input shape: [%d,%d,%d,%d]\n", tpu->input_shape[0], tpu->input_shape[1], tpu->input_shape[2], tpu->input_shape[3]);
    
    // ====== eeptpu init ok, only init one time in program.
    
    // ====== loop write input and forward if you have many images.
    do 
    {
        image_bytes img_orig;
        img_orig = eeptpu_write_input(path_image, tpu->input_shape[1], tpu->input_shape[2], tpu->input_shape[3], pixel_order);
        if (eepimg_empty(img_orig) == true)
        {
            printf("EEPTPU write input fail\n");
            return ret;
        }
        //printf("img_orig: %d,%d,%d\n", img_orig.c, img_orig.h, img_orig.w);
        
        std::vector<struct EEPTPU_RESULT> results;
        double st = get_current_time();
        double et;

        // sync forward mode
        ret = tpu->eeptpu_forward(results);
        if (ret < 0) 
        {
            printf("eeptpu forward fail, ret = %d\n", ret);
            return ret;
        }   
        
        et = get_current_time();
        printf("EEPTPU forward ok, cost time(hw+sw): %.3f ms\n", et-st);    
        unsigned int hwus = tpu->eeptpu_get_tpu_forward_time();
        printf("EEPTPU hardware cost: %.3f ms\n", (float)hwus/1000);
    
        if (results.size() == 0)
        {
            printf("Nothing detected. \n");
        }
        else
        {
            printf("Result count: %d\n", (int)results.size());
            for(unsigned int i = 0; i < results.size(); i++)
            {
                printf("    results[%d]: %d,%d,%d\n", i, results[i].shape[1], results[i].shape[2], results[i].shape[3]);
            }

            // post process for yolo3/yolo4/mobilenet-ssd, not suitable for yolo5
            ret = post_process_obj_detect(img_orig, results[0]);
            if (ret < 0) return ret;
            
            image_bytes final_image = draw_objects(img_orig, g_objects);
            eepimg_save("./objdet.jpg", final_image);
            printf("Saved result image to: ./objdet.jpg\n");
            if (eepimg_empty(final_image) == false) eepimg_free(final_image);        
        }

        eepimg_free(img_orig);
        
        for(unsigned int i = 0; i < results.size(); i++)
        {
            free(results[i].data);
        }
        results.clear();
        
    }while(0);
    
    // all images done, close eeptpu.
    tpu->eeptpu_close();        
    clear_objects();
    
    printf("\nAll done\n");
    
    return 0;
}
