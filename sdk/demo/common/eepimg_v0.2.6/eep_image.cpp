#include <string.h>
#include <vector>
#include "eep_image.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#endif

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#if defined(EEPIMG_ASCII_IMPLEMENTATION)
#include "eep_ascii.h"
#endif

using namespace  std;

////////////////////////////////////////////////////
// version: v0.2.6
/*
v0.2.6: update eepimg_load_image, add RGBA/BGRA.
v0.2.5: open function: eepimg_make_image_bytes; eepimg_copy_image;
v0.2.4: add eepimg_crop_resize()
v0.2.3: fixed bug(load image and crop/resize)
*/

/////////// internal functions /////////////////////
static void *xcalloc(size_t nmemb, size_t size);
static image_bytes eepimg_make_empty_image_bytes(int w, int h, int c);
image_bytes eepimg_make_image_bytes(int w, int h, int c);
image_bytes eepimg_copy_image(image_bytes p);
static void eepimg_rgbgr_image(image_bytes im);
////////////////////////////////////////////////////

/*
eepimg pixel default format:
    rgbrgbrgb....rgb...
*/

image_bytes eepimg_load_image(char *filename, int pixel_order)
{
    int w, h, c;
    
    int stbi = 0;
    if (pixel_order == EEPIMG_PIXEL_GRAY) stbi = STBI_grey;
    unsigned char *data = stbi_load(filename, &w, &h, &c, stbi);
    if (!data) 
    {
        char shrinked_filename[1024];
        if (strlen(filename) >= 1024) sprintf(shrinked_filename, "name is too long");
        else sprintf(shrinked_filename, "%s", filename);
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", shrinked_filename, stbi_failure_reason());
        return eepimg_make_empty_image_bytes(0, 0, 0);
    }
    
    image_bytes im;
    //printf("chw=%d,%d,%d, pixel=%d\n", c,h,w, pixel_order);
    
    if (pixel_order == EEPIMG_PIXEL_GRAY) c = 1;
    
    if (pixel_order == EEPIMG_PIXEL_DFT)
    {
        // default is gray/rgb/rgba
        im = eepimg_make_image_bytes(w, h, c);    
        memcpy(im.data, data, w*h*c);
    }
    else if (c == 1 && (pixel_order == EEPIMG_PIXEL_GRAY))
    {
        im = eepimg_make_image_bytes(w, h, 1);
        memcpy(im.data, data, w*h*1);
    }
    else if (c == 1 && ( (pixel_order == EEPIMG_PIXEL_RGB) || (pixel_order == EEPIMG_PIXEL_BGR) ) )
    {
        // gray to rgb 
        im = eepimg_make_image_bytes(w, h, 3);
        unsigned char* psrc = data;
        unsigned char* pdst = im.data;
        for (int i = 0; i < w*h; i++)
        {
            *pdst++ = *psrc;
            *pdst++ = *psrc;
            *pdst++ = *psrc++;
        }
    }
    else if (c == 3)   // rgb
    {
        // default is rgb
        
        if (pixel_order == EEPIMG_PIXEL_BGR)
        {
            im = eepimg_make_image_bytes(w, h, c); 
            memcpy(im.data, data, w*h*c);
            eepimg_rgbgr_image(im);
        }
        else if (pixel_order == EEPIMG_PIXEL_BGRA)
        {
            im = eepimg_make_image_bytes(w, h, 4); 
            unsigned char* psrc = data;
            unsigned char* pdst = im.data;
            for (int i = 0; i < w*h; i++)
            {
                pdst[2] = psrc[0];
                pdst[1] = psrc[1];
                pdst[0] = psrc[2];
                pdst[3] = 255.0;
                psrc += 3;
                pdst += 4;
            }
        }
        else if (pixel_order == EEPIMG_PIXEL_RGBA)
        {
            im = eepimg_make_image_bytes(w, h, 4); 
            unsigned char* psrc = data;
            unsigned char* pdst = im.data;
            for (int i = 0; i < w*h; i++)
            {
                pdst[0] = psrc[0];
                pdst[1] = psrc[1];
                pdst[2] = psrc[2];
                pdst[3] = 255.0;
                psrc += 3;
                pdst += 4;
            }
        }
        else    // default, rgb
        {
            im = eepimg_make_image_bytes(w, h, c); 
            memcpy(im.data, data, w*h*c);
        }
    }
    else if (c == 4)    // rgba
    {
        if (pixel_order == EEPIMG_PIXEL_RGB)
        {
            im = eepimg_make_image_bytes(w, h, 3);
            unsigned char* psrc = data;
            unsigned char* pdst = im.data;
            for (int i = 0; i < w*h; i++)
            {
                float alpha = psrc[3] / 255.0;
                pdst[0] = (1.0 - alpha) * 255 + alpha * (psrc[0]);
                pdst[1] = (1.0 - alpha) * 255 + alpha * (psrc[1]);
                pdst[2] = (1.0 - alpha) * 255 + alpha * (psrc[2]);
                psrc += 4;            
                pdst += 3;
            }
        }
        else if (pixel_order == EEPIMG_PIXEL_BGR)
        {
            im = eepimg_make_image_bytes(w, h, 3);
            unsigned char* psrc = data;
            unsigned char* pdst = im.data;
            for (int i = 0; i < w*h; i++)
            {
                float alpha = psrc[3] / 255.0;
                pdst[2] = (1.0 - alpha) * 255 + alpha * (psrc[0]);
                pdst[1] = (1.0 - alpha) * 255 + alpha * (psrc[1]);
                pdst[0] = (1.0 - alpha) * 255 + alpha * (psrc[2]);
                psrc += 4;
                pdst += 3;
            }
        }
        else if (pixel_order == EEPIMG_PIXEL_BGRA)
        {
            im = eepimg_make_image_bytes(w, h, 4);
            unsigned char* psrc = data;
            unsigned char* pdst = im.data;
            for (int i = 0; i < w*h; i++)
            {
                pdst[2] = psrc[0];
                pdst[1] = psrc[1];
                pdst[0] = psrc[2];
                pdst[3] = psrc[3];
                psrc += 4;            
                pdst += 4;
            }
        }
        else 
        {
            im = eepimg_make_image_bytes(w, h, 4);
            memcpy(im.data, data, w*h*4);
        }
    }
    else 
    {
        printf("Can't get stb image data.\n");
        return eepimg_make_empty_image_bytes(0, 0, 0);
    }
    
    //printf("read ok, im chw=%d,%d,%d\n", im.c, im.h, im.w);
    free(data);
    return im;
}

image_bytes eepimg_load_image(char *filename, int pixel_order, int layout)
{
    image_bytes im = eepimg_load_image(filename, pixel_order);
    if (layout == EEPIMG_LAYOUT_HWC) return im;
    else if (layout == EEPIMG_LAYOUT_CHW)
    {
        image_bytes chw;
        chw = eepimg_make_image_bytes(im.w, im.h, im.c);
        chw.layout = EEPIMG_LAYOUT_CHW;
        
        for (int c = 0; c < im.c; c++)
        {
            unsigned char* pdst = (unsigned char*)chw.data;
            pdst += (c * im.h * im.w);
            unsigned char* psrc = (unsigned char*)im.data;
            psrc += c;
            for (int i = 0; i < im.h * im.w; i++)
            {
                *pdst++ = *psrc;
                psrc += im.c;
            }
        }
        
        return chw;
    }
    else 
    {
        printf("Not supported layout: %d. (supported layout: 0-hwc; 1-chw.)\n", layout);
        return eepimg_make_empty_image_bytes(0, 0, 0);
    }
}

image_bytes eepimg_load_image(char *filename, int pixel_order, int resize_w, int resize_h, float crop_scale)
{
    if (resize_w == 0 || resize_h == 0) return eepimg_make_empty_image_bytes(0, 0, 0);
    
    image_bytes im = eepimg_load_image(filename, pixel_order);
    if (eepimg_empty(im) == true) return im;
    
    if (crop_scale == 0.0) crop_scale = 1.0;
    if (crop_scale != 1.0)
    {
        int src_h = im.h;
        int src_w = im.w;
        int short_edge = min(src_h, src_w);
        short_edge *= crop_scale;
        int yy = (src_h - short_edge) / 2;
        int xx = (src_w - short_edge) / 2;
        image_bytes im_crop = eepimg_crop(im, xx, yy, short_edge, short_edge);
        eepimg_free(im);
        
        if (im_crop.w == resize_w && im_crop.h == resize_h) return im_crop;
        else
        {
            image_bytes img_resized = eepimg_resize(im_crop, resize_w, resize_h);
            eepimg_free(im_crop);
            return img_resized;
        }
    }
    else
    {
        if (im.w == resize_w && im.h == resize_h) return im;
        else
        {
            image_bytes img_resized = eepimg_resize(im, resize_w, resize_h);
            eepimg_free(im);
            return img_resized;
        }
    }
    
    return im;
}

image_bytes eepimg_crop_resize(image_bytes im, int resize_w, int resize_h, float crop_scale)
{
    if (resize_w == 0 || resize_h == 0) return eepimg_make_empty_image_bytes(0, 0, 0);
    if (eepimg_empty(im) == true) return eepimg_copy_image(im);
    
    if (crop_scale == 0.0) crop_scale = 1.0;
    if (crop_scale != 1.0)
    {
        int src_h = im.h;
        int src_w = im.w;
        int short_edge = min(src_h, src_w);
        short_edge *= crop_scale;
        int yy = (src_h - short_edge) / 2;
        int xx = (src_w - short_edge) / 2;
        image_bytes im_crop = eepimg_crop(im, xx, yy, short_edge, short_edge);
        
        if (im_crop.w == resize_w && im_crop.h == resize_h) return im_crop;
        else
        {
            image_bytes img_resized = eepimg_resize(im_crop, resize_w, resize_h);
            eepimg_free(im_crop);
            return img_resized;
        }
    }
    else
    {
        if (im.w == resize_w && im.h == resize_h) 
        {
            return eepimg_copy_image(im);
        }
        else
        {
            image_bytes img_resized = eepimg_resize(im, resize_w, resize_h);
            return img_resized;
        }
    }
    
    return eepimg_copy_image(im);
}

void eepimg_from_buffer(image_bytes* im, unsigned char* data, int w, int h, int c)
{
    im->w = w;
    im->h = h;
    im->c = c;
    if (im->data != NULL) free(im->data);
    im->data = (unsigned char*)xcalloc(h * w * c, sizeof(unsigned char));
    memcpy(im->data, data, h*w*c);
}

void eepimg_free(image_bytes im)
{
    if(im.data)
    {
        free(im.data);
        im.data = NULL;
    }
}

bool eepimg_empty(image_bytes im)
{
    if (im.w == 0 || im.h == 0 || im.c == 0) return true;
    if (im.data == NULL) return true;
    else return false;
}

image_bytes eepimg_resize(image_bytes im, int w, int h)
{
    if (im.w == w && im.h == h) return eepimg_copy_image(im);
    
    image_bytes resized = eepimg_make_image_bytes(w, h, im.c);
    
    stbir_resize_uint8(im.data, im.w, im.h, 0, resized.data, w, h, 0, im.c);
    
    return resized;
}

/*
crop image from pixel(x0,y0), width is w, height is h.
pixel format is: 
    rgb:  rgbrgbrgb.....
    bgr:  bgrbgrbgr.....
    gray: ggggggggg.....
*/
image_bytes eepimg_crop(image_bytes im, int x0, int y0, int w, int h)
{
    if (x0==0 && y0==0 && im.w == w && im.h == h) 
    {
        return eepimg_copy_image(im);
    }

    if (x0 >= im.w) return eepimg_make_image_bytes(w,h,im.c);
    if ( (x0 + w) > im.w ) w = im.w - x0;
    if (y0 >= im.h) return eepimg_make_image_bytes(w,h,im.c);
    if ( (y0 + h) > im.h ) h = im.h - y0;

    image_bytes crop = eepimg_make_image_bytes(w, h, im.c);
    unsigned char* pdst = crop.data;
    
    int row_bytes = im.c * im.w;
    //int curr_h = 0;
    int copy_len = w * im.c;
    unsigned char* psrc = im.data + row_bytes*y0 + x0*im.c;
    
    for (int i = 0; i < h; i++)
    {
        memcpy(pdst, psrc, copy_len);
        pdst += copy_len;
        psrc += row_bytes;
    }

    return crop;
}

int eepimg_save(const char* filename, image_bytes im, bool swapRB)
{
    int ret = -1;
    const char* ptr = strrchr(filename, '.');
    if (ptr == NULL) return -1;
    
    image_bytes sm;
    if (swapRB) 
    {
        sm = eepimg_copy_image(im);
        eepimg_rgbgr_image(sm);
    }
    else sm = im;
            
    if (strcasecmp(ptr, ".bmp") == 0)
    {
        ret = stbi_write_bmp(filename, sm.w, sm.h, sm.c, sm.data);
    }
    else if (strcasecmp(ptr, ".png") == 0)
    {
        ret = stbi_write_png(filename, sm.w, sm.h, sm.c, sm.data, 0);
    }
    else if (strcasecmp(ptr, ".jpg") == 0)
    {
        ret = stbi_write_jpg(filename, sm.w, sm.h, sm.c, sm.data, 85);
    }
    else ret = -1;
    
    if (swapRB) eepimg_free(sm);
    return ret;
}

image_bytes eepimg_draw_box(image_bytes img, int x1, int y1, int x2, int y2, unsigned char b, unsigned char g, unsigned char r, unsigned char linewidth)
{
    image_bytes im = eepimg_copy_image(img);
    
    int i;
    if(x1 < 0) x1 = 0;
    if(x1 >= im.w) x1 = im.w-1;
    if(x2 < 0) x2 = 0;
    if(x2 >= im.w) x2 = im.w-1;
    if (x1 > x2) { i = x2; x2 = x1; x1 = i; }

    if(y1 < 0) y1 = 0;
    if(y1 >= im.h) y1 = im.h-1;
    if(y2 < 0) y2 = 0;
    if(y2 >= im.h) y2 = im.h-1;
    if (y1 > y2) { i = y2; y2 = y1; y1 = i; }
    
    if (linewidth < 1) linewidth = 1;
    
    int row_bytes = im.w * im.c;
    unsigned char gray = (b+g+r) / 3;
    
    unsigned char* ptr;

    // - (x1,y1)->(x2,y1)
    for (int n = 0; n < linewidth; n++)
    {
        if (y1+n >= im.h) break;
        ptr = im.data + (y1+n) * row_bytes + x1 * im.c;
        if (im.c == 3) for(i = x1; i <= x2; ++i) { *ptr++ = b; *ptr++ = g; *ptr++ = r; } 
        else if (im.c == 1) for(i = x1; i <= x2; ++i) { *ptr++ = gray; } 
    }
    
    // - (x1,y2)->(x2,y2)
    for (int n = 0; n < linewidth; n++)
    {
        if (y2+n >= im.h) break;
        ptr = im.data + (y2+n) * row_bytes + x1 * im.c;
        if (im.c == 3) for(i = x1; i <= x2; ++i) { *ptr++ = b; *ptr++ = g; *ptr++ = r; } 
        else if (im.c == 1) for(i = x1; i <= x2; ++i) { *ptr++ = gray; } 
    }
    
    // | (x1,y1)->(x1,y2)
    for (int n = 0; n < linewidth; n++)
    {
        if (x1+n > im.w) break;
        ptr = im.data + y1 * row_bytes + (x1+n) * im.c;
        if (im.c == 3) for(i = y1; i <= y2; ++i) { ptr[0] = b; ptr[1] = g; ptr[2] = r; ptr += row_bytes; } 
        else if (im.c == 1) for(i = y1; i <= y2; ++i) { ptr[0] = gray; ptr += row_bytes; } 
    }
    
    // | (x2,y1)->(x2,y2)
    y2 += (linewidth-1);
    if (y2 >= im.h) y2 = im.h - 1;
    for (int n = 0; n < linewidth; n++)
    {
        if (x2+n > im.w) break;
        ptr = im.data + y1 * row_bytes + (x2+n) * im.c;
        if (im.c == 3) for(i = y1; i <= y2; ++i) { ptr[0] = b; ptr[1] = g; ptr[2] = r; ptr += row_bytes; } 
        else if (im.c == 1) for(i = y1; i <= y2; ++i) { ptr[0] = gray; ptr += row_bytes; } 
    }

    return im;
}

static int get_suitable_char_height(int image_height)
{
#if defined(EEPIMG_ASCII_IMPLEMENTATION)
    int char_height = image_height * 0.05;
    int size_cnt = eepascii[0];
    if (size_cnt == 1) return 0;

    vector<int> sp;
    sp.resize(size_cnt - 1);
    for (int i = 0; i < size_cnt-1; i++)
    {
        sp[i] = (eepascii[1+i] + eepascii[1+i+1]) / 2;
    }
    
    int ret = 0;
    if (char_height <= sp[0]) ret = 0;
    else if (char_height > sp[sp.size()-1]) ret = sp.size();
    else 
    {
        for (unsigned int i = 1; i < sp.size(); i++)
        {
            if (char_height > sp[i-1] && char_height <= sp[i])
            {
                ret = i;
                break;
            }
        }
    }
    sp.clear();
    return ret;
#endif
    return 0;
}

static int c4toint(unsigned char* buf)
{
    return (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
}

int eepimg_get_text_height(int image_height)
{
#if defined(EEPIMG_ASCII_IMPLEMENTATION)
    int hid = get_suitable_char_height(image_height);
    return eepascii[1+hid];
#else
    return 0;
#endif
}

int eepimg_get_text_width(int image_height, char* text)
{
#if defined(EEPIMG_ASCII_IMPLEMENTATION)
    int char_height_idx = get_suitable_char_height(image_height);
    int pos = EAOFS_OFFSET_BLOCK + char_height_idx * STRIDE_OFFSET_BLOCK;
    int width = 0;
    for (unsigned int i = 0; i < strlen(text); i++)
    {
        int j = text[i] - 0x20;
        int ofs = c4toint((unsigned char*)eepascii + pos + j * 4);
        width += eepascii[ofs];
    }
    return width;
#else
    return 0;
#endif
}

image_bytes eepimg_draw_text(image_bytes im, int x, int y, char* text)
{
#if !defined(EEPIMG_ASCII_IMPLEMENTATION)
    printf("Not defined EEPIMG_ASCII_IMPLEMENTATION\n");
    return im;
#else
    // read from eepascii[]
    int char_height_idx = get_suitable_char_height(im.h);
    
    //printf("text: im.h=%d, char_height_idx=%d\n", im.h, char_height_idx);
    
    int pos = EAOFS_OFFSET_BLOCK + char_height_idx * STRIDE_OFFSET_BLOCK;
    
    int gx = x;
    int gy = y;
    
    image_bytes img = eepimg_copy_image(im);
    
    for (int i = 0; i < (int)strlen(text); i++)
    {
        int j = text[i] - 0x20;
        int ofs = c4toint((unsigned char*)eepascii + pos + j * 4);
        int char_w = eepascii[ofs];
        int char_h = eepascii[ofs+1];
        //int char_c = eepascii[ofs+2];
        unsigned char* char_data = (unsigned char*)eepascii + ofs + 3;
        
        // draw char to image.
        int cw = char_w;
        int ch = char_h;
        if (gx + cw >= img.w) cw = img.w - gx;
        if (gy + ch >= img.h) ch = img.h - gy;
        for (int m = 0; m < ch; m++)
        {
            unsigned char* ptrw = img.data + (gy+m) * img.w * img.c + gx * img.c;
            unsigned char* ptrs = char_data + m * char_w;
            for (int n = 0; n < cw; n++)
            {
                for (int c = 0; c < img.c; c++)
                {
                    *ptrw++ = *ptrs;
                }
                ptrs++;
            }
        }
        gx += cw;
        if (gx >= img.w) break;
    }
    
    return img;
#endif
}


void eepimg_imshow(char* title, image_bytes im)
{
    // todo
}

///////////////////////////////////////////////////////////////////////////////

static void calloc_error()
{
    fprintf(stderr, "Calloc error - possibly out of CPU RAM \n");
    exit(EXIT_FAILURE);
}

static void *xcalloc(size_t nmemb, size_t size) {
    void *ptr=calloc(nmemb,size);
    if(!ptr) {
        calloc_error();
    }
    memset(ptr, 0, nmemb * size);
    return ptr;
}

static image_bytes eepimg_make_empty_image_bytes(int w, int h, int c)
{
    image_bytes outimg;
    outimg.data = 0;
    outimg.h = h;
    outimg.w = w;
    outimg.c = c;
    return outimg;
}

image_bytes eepimg_make_image_bytes(int w, int h, int c)
{
    image_bytes outimg;// = eepimg_make_empty_image_bytes(w,h,c);
    outimg.c = c;
    outimg.h = h; 
    outimg.w = w;
    outimg.data = (unsigned char*)xcalloc(h * w * c, sizeof(unsigned char));
    return outimg;
}

image_bytes eepimg_copy_image(image_bytes p)
{
    image_bytes copy = p;
    copy.data = (unsigned char*)xcalloc(p.h * p.w * p.c, sizeof(unsigned char));
    memcpy(copy.data, p.data, p.h*p.w*p.c*sizeof(unsigned char));
    return copy;
}

static void eepimg_rgbgr_image(image_bytes im)
{
    int i;
    if (im.c == 3)
    {
        unsigned char* pdst = im.data;
        for(i = 0; i < im.w*im.h; ++i){
            unsigned char swap = *(pdst+2);
            *(pdst+2) = *pdst;
            *pdst = swap;
            pdst += 3;
        }
    }
    else if (im.c == 4) // rgba
    {
        unsigned char* pdst = im.data;
        for(i = 0; i < im.w*im.h; ++i){
            unsigned char swap = *(pdst+2);
            *(pdst+2) = *pdst;
            *pdst = swap;
            pdst += 4;
        }
    }
}

image_bytes eepimg_hwc2chw(image_bytes im)
{
    image_bytes chw;
    chw = eepimg_make_image_bytes(im.w, im.h, im.c);
    chw.layout = EEPIMG_LAYOUT_CHW;
    
    for (int c = 0; c < im.c; c++)
    {
        unsigned char* pdst = (unsigned char*)chw.data;
        pdst += (c * im.h * im.w);
        unsigned char* psrc = (unsigned char*)im.data;
        psrc += c;
        for (int i = 0; i < im.h * im.w; i++)
        {
            *pdst++ = *psrc;
            psrc += im.c;
        }
    }
    
    return chw;
}
