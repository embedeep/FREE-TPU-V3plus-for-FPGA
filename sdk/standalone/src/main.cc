//-----------------------------------------------------------------------------
// The confidential and proprietary information contained in this file may
// only be used by a person authorised under and to the extent permitted
// by a subsisting licensing agreement from EMBEDEEP Corporation.
//
//    (C) COPYRIGHT 2018-2022  EMBEDEEP Corporation.
//        ALL RIGHTS RESERVED
//
// This entire notice must be reproduced on all copies of this file
// and copies of this file may only be made by a person if such person is
// permitted to do so under the terms of a subsisting license agreement
// from EMBEDEEP Corporation.
//
//      Main version        : V0.0.1
//
//      Revision            : 2022-10-08
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "simplestl.h"
#include "public.h"
#include "utils.h"
#include "eeptpu/eeptpu_sa.h"
#include "eeptpu/nmat.h"
#include "config.h"
#include "post_process/post_process.h"
#include "resize.h"
#include "camera.h"
#include "net_data/eepnet.h"
#include "sd.h"
#if (FG_INPUT_DATA_SEPERATED == 1)
//#include "net_data/eepinput.h"
#endif

#include "platform.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xtime_l.h"

#include "sys_intr.h"
#include "dpdma_intr.h"

using namespace std;

//u32 rd_val;

u32 pic_hsize = IMG_WIDTH;
u32 pic_vsize = IMG_HEIGHT;

//RGB565 image data in memory
u32 mem_pic = 0x30000000;
u8 *img_data = (u8 *)mem_pic;

u8 r,g,b;
//RGB888 image data in memory
u8 *img_data_888 = (u8*)0x38000000;
u32 pic_tmp;

int choice, exit_flag;

//time
XTime tBegin,tEnd;
u32 tused;
u32 tused_forward;
u32 tused_det_out;

u32 pic_cnt = 0;
u32 runtimer;
char pic_name[128];

unsigned int x, y, w, h;
int inlen = 0;

//DP
#ifdef EEP_DP_ENABLE
extern XScuGic Intc; //GIC
extern XDpDma DpDma;
extern XDpPsu DpPsu;
extern XAVBuf AVBuf;
extern Run_Config RunCfg;

u8 GFrame[BUFFERSIZE] __attribute__ ((__aligned__(256)));
u32 argb;
u32 *ARGB = (u32*) GFrame;
#endif

//TPU Config
EEPTPU_SA eepsa;
u32 waddr;
u32 sd_input_addr;
u32 tpu_hwbase2;
u32 tpu_hwbase3;
u32 tpu_algbase;
u8 *eepinput_addr;
u8 *eepnet;

static int get_input_data(EEPTPU_SA* eepsa, unsigned char* addr_img, int img_w, int img_h, unsigned char* inbuf, int* inlen)
{
	int net_in_c = eepsa->addr_in.shape[1];
	int net_in_h = eepsa->addr_in.shape[2];
	int net_in_w = eepsa->addr_in.shape[3];
	int net_in_exp = eepsa->addr_in.exp;
	float* mean = eepsa->mean.data();
	float* norm = eepsa->norm.data();

	// get input image
	// store camera image data, format is bgrbgrbgr.... or rgbrgbrgbrgb....
	unsigned char* inputbuf = addr_img;
	// resize image.
	unsigned char* inbuf_resized = (unsigned char*)0x39000000; //(unsigned char*)malloc(net_in_w*net_in_h*net_in_c);
	if (inbuf_resized == NULL) return eeperr_Malloc;
	if (net_in_c == 3)
	{
		resize_bilinear_c3(inputbuf, img_w, img_h, inbuf_resized, net_in_w, net_in_h);
	}
	else if (net_in_c == 1)
	{
		resize_bilinear_c1(inputbuf, img_w, img_h, inbuf_resized, net_in_w, net_in_h);
	}

	// set to hw format
	// inputbuf:  hwc
	*inlen = net_in_w*net_in_h*32;
	memset(inbuf, 0, *inlen);
	unsigned char *pdst = inbuf;
	unsigned char *psrc = inbuf_resized;
	int mul_val = pow(2, net_in_exp);
	if (net_in_c == 3)
	{
		vector<float> mul2;
		for (int k = 0; k < net_in_c; k++) mul2.push_back(norm[k]*mul_val);
		for (int hw = 0; hw < net_in_w*net_in_h; hw++)
		{
			short us;

        #if 0 // src data is bgrbgrbgr....; dst use bgr
			us = round( ((float)*psrc - mean[0]) * mul2[0]);
			psrc++;
			*pdst++ = us & 0xff;
			*pdst++ = (us >> 8) & 0xff;

			us = round( ((float)*psrc - mean[1]) * mul2[1]);
			psrc++;
			*pdst++ = us & 0xff;
			*pdst++ = (us >> 8) & 0xff;

			us = round( ((float)*psrc - mean[2]) * mul2[2]);
			psrc++;
			*pdst++ = us & 0xff;
			*pdst++ = (us >> 8) & 0xff;
			pdst += 26;
        #else  // src data is rgbrgbrgb....; dst use bgr
			us = round( ((float)*psrc++ - mean[0]) * mul2[0]);
			pdst[4] = us & 0xff;
			pdst[5] = (us >> 8) & 0xff;

			us = round( ((float)*psrc++ - mean[1]) * mul2[1]);
			pdst[2] = us & 0xff;
			pdst[3] = (us >> 8) & 0xff;

			us = round( ((float)*psrc++ - mean[2]) * mul2[2]);
			pdst[0] = us & 0xff;
			pdst[1] = (us >> 8) & 0xff;
			pdst += 32;
        #endif
		}
		mul2.clear();
	}
	else if (net_in_c == 1)
	{
		float mul2 = norm[0] * mul_val;
		for (int hw = 0; hw < net_in_w*net_in_h; hw++)
		{
			// gray
			unsigned short us;
			us = round( ((float)*psrc++ - mean[0]) * mul2);
			*pdst++ = us & 0xff;
			*pdst++ = (us >> 8) & 0xff;

			pdst += 30;
		}
	}
	// free(inbuf_resized);
	return 0;
}

static void tpu_forward()
{
	u32 ret;
	u32 rd_val;

    EEPTPU_BASEADDR0_REG=waddr;
#ifdef EEP_DEBUG_INFO
    rd_val = EEPTPU_BASEADDR0_REG;
	xil_printf("EEPTPU_BASEADDR0_REG value: 0x%08x\r\n", rd_val);
#endif
    EEPTPU_BASEADDR1_REG=sd_input_addr;
#ifdef EEP_DEBUG_INFO
    rd_val = EEPTPU_BASEADDR1_REG;
    xil_printf("EEPTPU_BASEADDR1_REG value: 0x%08x\r\n", rd_val);
#endif
    EEPTPU_BASEADDR2_REG=tpu_hwbase2;
#ifdef EEP_DEBUG_INFO
    rd_val = EEPTPU_BASEADDR2_REG;
    xil_printf("EEPTPU_BASEADDR2_REG value: 0x%08x\r\n", rd_val);
#endif
    EEPTPU_BASEADDR3_REG=tpu_hwbase3;
#ifdef EEP_DEBUG_INFO
    rd_val = EEPTPU_BASEADDR3_REG;
    xil_printf("EEPTPU_BASEADDR3_REG value: 0x%08x\r\n", rd_val);
#endif
    EEPTPU_ALGOADDR_REG=tpu_algbase;
#ifdef EEP_DEBUG_INFO
    rd_val = EEPTPU_ALGOADDR_REG;
    xil_printf("EEPTPU_ALGOADDR_REG value: 0x%08x\r\n", rd_val);
#endif
    EEPTPU_STARTUP_REG=0x11;

    rd_val=EEPTPU_STATUS_REG;
    while( (rd_val & 0x80000000) != 0x80000000 )
    {
    	rd_val=EEPTPU_STATUS_REG;
    	ret = 0;
    }
}

#ifdef EEP_DP_ENABLE
void init_intr_sys(void)
{
	Init_Intr_System(&Intc);//initialize global interrupt source
	Dpdma_init(&RunCfg ,&Intc);//setup dp channel
	Dpdma_Setup_Intr_System(&RunCfg);//enable dp channel
	Setup_Intr_Exception(&Intc);//enable global interrupt source
}
#endif

int main(int argc, char* argv[])
{
    int ret;
    u32 rd_val;

    vector< ncnn::Mat > outputs;
    extern int yolo3_detection_output_forward(const std::vector<ncnn::Mat>& bottom_blobs, std::vector<ncnn::Mat>& top_blobs);
    // yolo3 last layer.
    // ====== object detect process ======
    static const char *class_names_default[] = {"background","person","bicycle","car","motorbike","aeroplane","bus","train",
    		"truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep",
			"cow","elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard",
			"sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle","wine glass",
			"cup","fork","knife","spoon,bowl","banana,apple","sandwich","orange","broccoli","carrot","hot dog","pizza,donut",
			"cake","chair","sofa","pottedplant","bed","diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone",
			"microwave","oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"};
    char** class_names = (char **)class_names_default;
    vector<ncnn::Mat> top_blobs;
    ncnn::Mat blob_out;

    init_platform();

    xil_printf("Loading program ...... \r\n");

#ifdef SD_CARD_IS_READY
    // Initial SD card
    SD_Init();
#endif
#ifdef EEP_DEBUG_INFO
	xil_printf("Configure Camera Register...\r\n");
#endif
#ifdef EEP_DVP_CAMERA
	// Configure DVP camera
	I2C_config_init_720p(pic_hsize, pic_vsize);
#endif
#ifdef EEP_DEBUG_INFO
	xil_printf("Configure Camera Register Done\r\n");
#endif

	Xil_DCacheDisable();
	//Xil_ICacheDisable();
#ifdef EEP_DP_ENABLE
	graphic_buffer_init(GFrame);
	init_intr_sys();
#endif

	//EEPTPU_SA eepsa;

    // Initial EEP TPU Config information from array eepnet_config
    ret = eepsa.eeptpu_init((unsigned char *)0x10000000, 0, eepnet_config, sizeof(eepnet_config));
    if (ret < 0)
    {
        xil_printf("eeptpu_init fail, ret = %d\r\n", ret);
        return ret;
    }
    Xil_DCacheFlush();

    //TPU Config
    waddr         = eepsa.hwbase0;
    sd_input_addr = eepsa.hwbase1;
    tpu_hwbase2   = eepsa.hwbase2;
    tpu_hwbase3   = eepsa.hwbase3;
    tpu_algbase   = eepsa.addr_alg;
    eepinput_addr = (u8 *)sd_input_addr;
    eepnet        = (u8 *)waddr;

#ifdef EEP_DEBUG_INFO
    xil_printf("net_addr:0x%08x\r\n", waddr);
    xil_printf("input_addr:0x%08x\r\n", sd_input_addr);
    xil_printf("tpu_algbase:0x%08x\r\n", tpu_algbase);
#endif
#ifdef SD_CARD_IS_READY
    //load net data from sd
    print("Load Net Data From SD Card...\r\n");
    file_read("eepnet.mem",eepnet, NET_SIZE);
    Xil_DCacheFlush();
    xil_printf("Load Net Done\r\n");
#endif

#ifdef EEP_DVP_CAMERA
    tused = dvp_capture(0x000f1013, pic_hsize*2, pic_vsize);
#endif
	exit_flag = 0;
	while(exit_flag != 1) {
		xil_printf("###################################################\r\n");
    	xil_printf("Choose Feature to Test:\r\n");
    	xil_printf("1: Get 1 Frame\r\n");
    	xil_printf("2: Forward Result\r\n");
        #ifdef SD_CARD_IS_READY
    	xil_printf("3: Save Image to SD Card\r\n");
        #endif
    	xil_printf("4: Read Test Image\r\n");
    	xil_printf("5: Run Demo\r\n");
    	xil_printf("0: Exit\r\n");
    	xil_printf("Enter the Number:");
    	choice = inbyte();
        if (isalpha(choice)) {
        	choice = toupper(choice);
        }
    	xil_printf("%c\r\n", choice);

		switch(choice) {
			case '0':
				exit_flag = 1;
				break;
			case '1':
            #ifdef EEP_DVP_CAMERA
                // Take a pictrue
				tused = dvp_capture(0x00011013, pic_hsize*2, pic_vsize);
			    Xil_DCacheFlush();
                #ifdef EEP_DEBUG_INFO
			    xil_printf("capture time is %d us\r\n", tused);
                #endif
            #else
				xil_printf("No camera !!! \r\n");
            #endif

            #ifdef IMG_RGB565
			    //RGB565->RGB888
			    RGB565toRGB888(pic_hsize, pic_vsize, img_data, img_data_888);
                #ifdef EEP_DP_ENABLE
			    //dp display
			    dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
                #endif
            #else
			    //GRAY->RGB
			    void GRAYtoRGB888(pic_hsize, pic_vsize, img_data, img_data_888);
                #ifdef EEP_DP_ENABLE
                //dp display
                dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
                #endif
            #endif

			    Xil_DCacheFlush();
			    xil_printf("Image Capture Done\r\n");
                // pre-process image data, include resize/mean/normalize
			    get_input_data(&eepsa, img_data_888, pic_hsize, pic_vsize, (unsigned char*)eepsa.hwbase1/*inbuf*/, &inlen);	// image_addr, img_h, img_w : change to yours.

				break;
			case '2':
			    // forward
			    xil_printf("Forwarding...\r\n");
			    XTime_GetTime(&tBegin);
			    tpu_forward();
			    XTime_GetTime(&tEnd);
			    tused_forward = ((tEnd-tBegin)*1000000)/(COUNTS_PER_SECOND);
			    if (ret < 0)
			    {
			        xil_printf("forward fail. ret = %d\r\n", ret);
			        return ret;
			    }
            #ifdef EEP_DEBUG_INFO
                runtimer = EEPTPU_RUNTIMER_REG;
                xil_printf("EEPTPU Run timer value: 0x%08x\r\n", runtimer);
            #endif
			    // read output.
			    ret = eepsa.read_forward_result(outputs);
			    if (ret < 0) return ret;

			    // ------- Post process --------
			#if (NET_TYPE == NetType_Classify)
			    std::vector< std::pair<float, int> > top_list;
			    ret = get_topk(outputs[0], 5, top_list);
			    if (ret < 0) return ret;

			    printf("Classify: Result (top 5): [%d %d %d %d %d] [%f %f %f %f %f]\n",
			            top_list[0].second, top_list[1].second, top_list[2].second, top_list[3].second, top_list[4].second,
			            top_list[0].first, top_list[1].first, top_list[2].first, top_list[3].first, top_list[4].first);
			    top_list.clear();
			#endif

			#if (YOLO3_DETECTION_OUTPUT)

                #ifdef EEP_DEBUG_INFO
			    xil_printf("Calculate yolo3_detection_output layer...\r\n");
                #endif
			    Xil_DCacheEnable();
			    XTime_GetTime(&tBegin);
			    ret = yolo3_detection_output_forward(outputs, top_blobs);
			    XTime_GetTime(&tEnd);
			    tused_det_out = ((tEnd-tBegin)*1000000)/(COUNTS_PER_SECOND);
			    Xil_DCacheFlush();
			    Xil_DCacheDisable();
			    if (ret < 0) {
			    	xil_printf("yolo3 detection output fail\r\n");
			    	top_blobs.clear();
				    outputs.clear();
				    eepsa.eeptpu_deinit();
			    	break;
			    }
                #ifdef EEP_DEBUG_INFO
			    xil_printf("Forwarding done.\r\n");
                #endif
			    xil_printf("forward time is %d us\r\n", tused_forward);
			    xil_printf("detection output time is %d us\r\n", tused_det_out);
			    // show result.
			    blob_out = top_blobs[0];
			#ifdef IMG_RGB565
			    for (int i = 0; i < blob_out.h; i++)
			    {
			        const float *values = blob_out.row(i);

			        int label = values[0];
			        float prob = values[1];
			        float scale_x = values[2];
			        float scale_y = values[3];
			        float scale_w = values[4];
			        float scale_h = values[5];

			        if(scale_x < 0) scale_x = 0;
			        if(scale_y < 0) scale_y = 0;
			        if(scale_w < 0) scale_w = 0;
			        if(scale_h < 0) scale_h = 0;
				    x = scale_x * pic_hsize;
				    y = scale_y * pic_vsize;
				    w = scale_w * pic_hsize;
				    h = scale_h * pic_vsize;
				    //draw object
				    draw_object(pic_hsize, x, y, w, h, img_data_888);

			        printf("Obj[%d]: %2d(%10s)  %f; At (%f,%f)  %f x %f\n", i, label, class_names[label], prob, scale_x, scale_y, scale_w, scale_h);
			    }
			#else
			    for (int i = 0; i < blob_out.h; i++)
			    {
			        const float *values = blob_out.row(i);

			        int label = values[0];
			        float prob = values[1];
			        float scale_x = values[2];
			        float scale_y = values[3];
			        float scale_w = values[4];
			        float scale_h = values[5];

			        if(scale_x < 0) scale_x = 0;
			        if(scale_y < 0) scale_y = 0;
			        if(scale_w < 0) scale_w = 0;
			        if(scale_h < 0) scale_h = 0;

				    x = scale_x * pic_hsize;
				    y = scale_y * pic_vsize;
				    w = scale_w * pic_hsize;
				    h = scale_h * pic_vsize;

			        xil_printf("Obj[%d]: %2d(%10s)  %f; At (%f,%f)  %f x %f\n", i, label, class_names[label], prob, scale_x, scale_y, scale_w, scale_h);
			    }
            #endif
			    top_blobs.clear();
			#endif

			    outputs.clear();

			    eepsa.eeptpu_deinit();
          
			#ifdef EEP_DP_ENABLE
			    // dp display
			    dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
	    	#endif
				break;
            #ifdef SD_CARD_IS_READY
			case '3':
				pic_cnt++;
				sprintf(pic_name, "pic_%d.bmp", pic_cnt);
				if (pic_vsize == 480) {
					bmp_write(pic_name, (char *)&BMODE_640x480, (char *)img_data_888);
				} else if (pic_vsize == 720) {
					bmp_write(pic_name, (char *)&BMODE_1280x720, (char *)img_data_888);
				} else if (pic_vsize == 1080) {
					bmp_write(pic_name, (char *)&BMODE_1920x1080, (char *)img_data_888);
				} else {
					xil_printf("Unsupport resolution!!!\r\n");
				}
				break;
            #endif
			case '4':
			#ifdef SD_CARD_IS_READY
			    // read tpu input data from sdcard
				file_read("eepinput.mem",eepinput_addr, INPUTDATA_SIZE);
			    Xil_DCacheFlush();
			#else
			    ret = eepsa.eeptpu_input(eepinput, sizeof(eepinput));
			    if (ret < 0)
				{
					xil_printf("eeptpu_input fail, ret = %d\n", ret);
					return ret;
				}
			#endif
				break;
			case '5':
				while(1)
				{
				#ifdef EEP_DVP_CAMERA
					tused = dvp_capture(0x00011013, pic_hsize*2, pic_vsize);
					Xil_DCacheFlush();
					#ifdef EEP_DEBUG_INFO
					xil_printf("capture time is %d us\r\n", tused);
					#endif
				#else
					xil_printf("No camera !!! \r\n");
				#endif

				#ifdef IMG_RGB565
					//RGB565->RGB888
					RGB565toRGB888(pic_hsize, pic_vsize, img_data, img_data_888);
					#ifdef EEP_DP_ENABLE
					//dp display
					dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
				    #endif
				#else
					//GRAY->RGB
				    void GRAYtoRGB888(pic_hsize, pic_vsize, img_data, img_data_888);
	                #ifdef EEP_DP_ENABLE
	                //dp display
	                dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
	                #endif
				#endif

					Xil_DCacheFlush();
					xil_printf("Image Capture Done\r\n");

					// inbuf = (unsigned char*)eepsa.hwbase1;
					get_input_data(&eepsa, img_data_888, pic_hsize, pic_vsize, (unsigned char*)eepsa.hwbase1/*inbuf*/, &inlen);	// image_addr, img_h, img_w : change to yours.

				    // forward
				    xil_printf("Forwarding...\r\n");
				    XTime_GetTime(&tBegin);
				    tpu_forward();
				    XTime_GetTime(&tEnd);
				    tused_forward = ((tEnd-tBegin)*1000000)/(COUNTS_PER_SECOND);
				    if (ret < 0)
				    {
				        xil_printf("forward fail. ret = %d\r\n", ret);
				        return ret;
				    }
	            #ifdef EEP_DEBUG_INFO
	                runtimer = EEPTPU_RUNTIMER_REG;
	                xil_printf("EEPTPU Run timer value: 0x%08x\r\n", runtimer);
	            #endif
				    // read output.

				    ret = eepsa.read_forward_result(outputs);
				    if (ret < 0) return ret;

				    // ------- Post process --------
				#if (NET_TYPE == NetType_Classify)
				    std::vector< std::pair<float, int> > top_list;
				    ret = get_topk(outputs[0], 5, top_list);
				    if (ret < 0) return ret;

				    printf("Classify: Result (top 5): [%d %d %d %d %d] [%f %f %f %f %f]\n",
				            top_list[0].second, top_list[1].second, top_list[2].second, top_list[3].second, top_list[4].second,
				            top_list[0].first, top_list[1].first, top_list[2].first, top_list[3].first, top_list[4].first);
				    top_list.clear();
				#endif

				#if (YOLO3_DETECTION_OUTPUT)

	                #ifdef EEP_DEBUG_INFO
				    xil_printf("Calculate yolo3_detection_output layer...\r\n");
	                #endif
				    Xil_DCacheEnable();
				    XTime_GetTime(&tBegin);
				    ret = yolo3_detection_output_forward(outputs, top_blobs);
				    XTime_GetTime(&tEnd);
				    tused_det_out = ((tEnd-tBegin)*1000000)/(COUNTS_PER_SECOND);
				    Xil_DCacheFlush();
				    Xil_DCacheDisable();
				    if (ret < 0) {
				    	xil_printf("yolo3 detection output fail\r\n");
				    	top_blobs.clear();
					    outputs.clear();
					    eepsa.eeptpu_deinit();
				    	continue;
				    }
	                #ifdef EEP_DEBUG_INFO
				    xil_printf("Forwarding done.\r\n");
	                #endif
				    xil_printf("forward time is %d us\r\n", tused_forward);
				    xil_printf("detection output time is %d us\r\n", tused_det_out);
				    // show result.
				    blob_out = top_blobs[0];
				#ifdef IMG_RGB565
				    for (int i = 0; i < blob_out.h; i++)
				    {
				        const float *values = blob_out.row(i);

				        int label = values[0];
				        float prob = values[1];
				        float scale_x = values[2];
				        float scale_y = values[3];
				        float scale_w = values[4];
				        float scale_h = values[5];

				        if(scale_x < 0) scale_x = 0;
				        if(scale_y < 0) scale_y = 0;
				        if(scale_w < 0) scale_w = 0;
				        if(scale_h < 0) scale_h = 0;
					    x = scale_x * pic_hsize;
					    y = scale_y * pic_vsize;
					    w = scale_w * pic_hsize;
					    h = scale_h * pic_vsize;
					    //draw object
					    draw_object(pic_hsize, x, y, w, h, img_data_888);

				        printf("Obj[%d]: %2d(%10s)  %f; At (%f,%f)  %f x %f\n", i, label, class_names[label], prob, scale_x, scale_y, scale_w, scale_h);
				    }
				#else
				    for (int i = 0; i < blob_out.h; i++)
				    {
				        const float *values = blob_out.row(i);

				        int label = values[0];
				        float prob = values[1];
				        float scale_x = values[2];
				        float scale_y = values[3];
				        float scale_w = values[4];
				        float scale_h = values[5];

				        if(scale_x < 0) scale_x = 0;
				        if(scale_y < 0) scale_y = 0;
				        if(scale_w < 0) scale_w = 0;
				        if(scale_h < 0) scale_h = 0;

					    x = scale_x * pic_hsize;
					    y = scale_y * pic_vsize;
					    w = scale_w * pic_hsize;
					    h = scale_h * pic_vsize;

				        xil_printf("Obj[%d]: %2d(%10s)  %f; At (%f,%f)  %f x %f\n", i, label, class_names[label], prob, scale_x, scale_y, scale_w, scale_h);
				    }
	            #endif
				    top_blobs.clear();
				#endif

				    outputs.clear();

				    eepsa.eeptpu_deinit();

				#ifdef EEP_DP_ENABLE
				    // dp display
				    dp_display(pic_hsize, pic_vsize, img_data_888, ARGB);
		    	#endif
				}
				break;
			default:
				break;
		}
    #ifdef EEP_DEBUG_INFO
		if(exit_flag != 1) {
			print("Press any key to return to main menu\r\n");
			inbyte();
		}
    #endif
	}

    cleanup_platform();

    return 0;
}

