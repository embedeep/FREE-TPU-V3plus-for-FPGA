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

#ifndef SRC_SD_H_
#define SRC_SD_H_

#define BYTES_PIXEL 3

typedef struct{
	char bm_header[2] ;	/* bmp header, always 0x4d, 0x42 */
	char bm_len[4] ;	/* bmp file length, bmp header plus data in byte */
	char reserved[4] ;	/* reserved always 0 */
	char offset[4] ;	/* bmp data offset */
	/* BMP information header, 40 bytes */
	char bm_infolen[4] ;	/* information header length */
	char bm_width[4] ;		/* bmp frame width */
	char bm_height[4] ;		/* bmp frame height */
	char color_plane[2] ;   /* color plane always 1 */
	char pixel_width[2] ;	/* pixel width, 24bit, 32bit, 16bit and etc */
	char compress[4] ;		/* compressed information 0: not compressed 1: 8bit RLE compressed 2: 4bit RLE compressed 3: bit fields*/
	char bm_bytes[4] ;		/* bmp frame total length in byte, for 800*600,24bit, 800*600*3byte= 1440000 bytes */
	char meter_width[4] ;	/* bmp frame width in pixel/meter, could be ignored */
	char meter_height[4] ;	/* bmp frame height in pixel/meter, could be ignored */
	char color_index[4] ;	/* color index number, if set 0, will use all color */
	char index_num[4] ;		/* importance color index number, if set 0, all are important */
}BmpMode;


static const BmpMode BMODE_640x480 = {
	.bm_header = {0x42, 0x4d},
	.bm_len = {0x36, 0x10, 0x0e, 0x00},		/* file length 921600+54 bytes */
	.reserved = {0x00, 0x00, 0x00, 0x00},
	.offset = {0x36, 0x00, 0x00, 0x00},		/* 54 bytes */
    .bm_infolen = {0x28, 0x00, 0x00, 0x00},	/* 40 bytes */
    .bm_width = {0x80, 0x02, 0x00, 0x00},	/* width 640 */
    .bm_height = {0xe0, 0x01, 0x00, 0x00},	/* height 480 */
    .color_plane = {0x01, 0x00},
    .pixel_width = {0x18, 0x00},			/* pixel 24 bit true color */
    .compress = {0x00, 0x00, 0x00, 0x00},	/* not compressed */
    .bm_bytes = {0x00, 0x10, 0x0e, 0x00},	/* frame length 921600 bytes */
    .meter_width = {0x00, 0x00, 0x00, 0x00},
    .meter_height = {0x00, 0x00, 0x00, 0x00},
    .color_index = {0x00, 0x00, 0x00, 0x00},
    .index_num = {0x00, 0x00, 0x00, 0x00}
};

static const BmpMode BMODE_1280x720 = {
	.bm_header = {0x42, 0x4d},
	.bm_len = {0x36, 0x10, 0x0e, 0x00},		/* file length 921600+54 bytes */
	.reserved = {0x00, 0x00, 0x00, 0x00},
	.offset = {0x36, 0x00, 0x00, 0x00},		/* 54 bytes */
    .bm_infolen = {0x28, 0x00, 0x00, 0x00},	/* 40 bytes */
    .bm_width = {0x00, 0x05, 0x00, 0x00},	/* width 640 */
    .bm_height = {0xd0, 0x02, 0x00, 0x00},	/* height 480 */
    .color_plane = {0x01, 0x00},
    .pixel_width = {0x18, 0x00},			/* pixel 24 bit true color */
    .compress = {0x00, 0x00, 0x00, 0x00},	/* not compressed */
    .bm_bytes = {0x00, 0x10, 0x0e, 0x00},	/* frame length 921600 bytes */
    .meter_width = {0x00, 0x00, 0x00, 0x00},
    .meter_height = {0x00, 0x00, 0x00, 0x00},
    .color_index = {0x00, 0x00, 0x00, 0x00},
    .index_num = {0x00, 0x00, 0x00, 0x00}
};

static const BmpMode BMODE_1920x1080 = {
	.bm_header = {0x42, 0x4d},
	.bm_len = {0x36, 0x10, 0x0e, 0x00},		/* file length 921600+54 bytes */
	.reserved = {0x00, 0x00, 0x00, 0x00},
	.offset = {0x36, 0x00, 0x00, 0x00},		/* 54 bytes */
    .bm_infolen = {0x28, 0x00, 0x00, 0x00},	/* 40 bytes */
    .bm_width = {0x80, 0x07, 0x00, 0x00},	/* width 640 */
    .bm_height = {0x38, 0x04, 0x00, 0x00},	/* height 480 */
    .color_plane = {0x01, 0x00},
    .pixel_width = {0x18, 0x00},			/* pixel 24 bit true color */
    .compress = {0x00, 0x00, 0x00, 0x00},	/* not compressed */
    .bm_bytes = {0x00, 0x10, 0x0e, 0x00},	/* frame length 921600 bytes */
    .meter_width = {0x00, 0x00, 0x00, 0x00},
    .meter_height = {0x00, 0x00, 0x00, 0x00},
    .color_index = {0x00, 0x00, 0x00, 0x00},
    .index_num = {0x00, 0x00, 0x00, 0x00}
};

int SD_Init();
void bmp_read(char * bmp,unsigned char *frame,unsigned int stride);
void bmp_write(char * name, char *head_buf, char *data_buf);
int file_read(char * bmp,unsigned char *frame, unsigned int len);
int file_write(char * name, char *head_buf, int len);

#endif /* SRC_SD_H_ */
