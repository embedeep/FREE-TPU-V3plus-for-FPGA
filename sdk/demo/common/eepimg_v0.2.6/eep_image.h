#ifndef _EEP_IMAGE_H
#define _EEP_IMAGE_H

#include <stdio.h>

// eep ascii define.
#define  EEPIMG_ASCII_IMPLEMENTATION

// eepim pixel type
#define  EEPIMG_PIXEL_DFT      0
#define  EEPIMG_PIXEL_RGB      1
#define  EEPIMG_PIXEL_BGR      2
#define  EEPIMG_PIXEL_GRAY     3
#define  EEPIMG_PIXEL_RGBA     4
#define  EEPIMG_PIXEL_BGRA     5

#define EEPIMG_LAYOUT_HWC      0
#define EEPIMG_LAYOUT_CHW      1

struct st_image_bytes {
    int w;
    int h;
    int c;
    unsigned char *data;
    int layout;
    
    st_image_bytes() :
        w(0),
        h(0),
        c(0),
        data(NULL),
        layout(EEPIMG_LAYOUT_HWC)
    { }
};
typedef struct st_image_bytes image_bytes;

// functions.
image_bytes eepimg_load_image(char *filename, int pixel_order);
image_bytes eepimg_load_image(char *filename, int pixel_order, int layout);
image_bytes eepimg_load_image(char *filename, int pixel_order, int resize_w, int resize_h, float crop_scale);
image_bytes eepimg_crop_resize(image_bytes im, int resize_w, int resize_h, float crop_scale);
image_bytes eepimg_make_image_bytes(int w, int h, int c);
image_bytes eepimg_copy_image(image_bytes p);
image_bytes eepimg_hwc2chw(image_bytes im);
void eepimg_from_buffer(image_bytes* im, unsigned char* data, int w, int h, int c);
void eepimg_free(image_bytes im);
bool eepimg_empty(image_bytes im);
image_bytes eepimg_resize(image_bytes im, int w, int h);
image_bytes eepimg_crop(image_bytes im, int x0, int y0, int w, int h);
int eepimg_save(const char* filename, image_bytes im, bool swapRB = true);
image_bytes eepimg_draw_box(image_bytes im, int x1, int y1, int x2, int y2, unsigned char b, unsigned char g, unsigned char r, unsigned char linewidth = 1);
image_bytes eepimg_draw_text(image_bytes im, int x, int y, char* text);
int eepimg_get_text_height(int image_height);
int eepimg_get_text_width(int image_height, char* text);
void eepimg_imshow(char* title, image_bytes im); // todo

#endif
