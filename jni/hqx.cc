/*
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>
#include "common.h"
#include "hqx.h"


#define LOG_INFO(fmt, args...)                                     \
do {                                                               \
    fprintf(stdout, "[%s:%d] " fmt " \n", __FILE__, __LINE__, ##args);    \
    fflush(stdout);                                                \
} while(0)

#define ABORT()                                                    \
do {                                                               \
    fprintf(stdout, "abort at [%s:%d] \n", __FILE__, __LINE__);    \
    fflush(stdout);                                                \
    abort();                                                       \
} while(0)

void read_png_file(char *filename, int& width, int& height, png_byte& color_type, png_byte& bit_depth, png_bytep*& row_pointers ) {
    FILE *fp = fopen(filename, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) ABORT();

    png_infop info = png_create_info_struct(png);
    if(!info) ABORT();

    if(setjmp(png_jmpbuf(png))) ABORT();

    png_init_io(png, fp);

    png_read_info(png, info);

    width      = png_get_image_width(png, info);
    height     = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth  = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if(color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    if (row_pointers) ABORT();

    row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
    }

    png_read_image(png, row_pointers);

    fclose(fp);

    png_destroy_read_struct(&png, &info, NULL);
}

void write_png_file(char *filename, int width, int height, png_bytep* row_pointers ) {
    int y;

    FILE *fp = fopen(filename, "wb");
    if(!fp) ABORT();

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) ABORT();

    png_infop info = png_create_info_struct(png);
    if (!info) ABORT();

    if (setjmp(png_jmpbuf(png))) ABORT();

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
            png,
            info,
            width, height,
            8,
            PNG_COLOR_TYPE_RGBA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
            );
    png_write_info(png, info);

    // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
    // Use png_set_filler().
    //png_set_filler(png, 0, PNG_FILLER_AFTER);

    if (!row_pointers) ABORT();

    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    for(int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    fclose(fp);

    png_destroy_write_struct(&png, &info);
}

#if 0
void process_png_file() {
    for(int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for(int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            // Do something awesome for each pixel here...
            //printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1], px[2], px[3]);
        }
    }
}
#endif

static inline uint32_t swapByteOrder(uint32_t ui)
{
    return (ui >> 24) | ((ui << 8) & 0x00FF0000) | ((ui >> 8) & 0x0000FF00) | (ui << 24);
}

int main(int argc, char *argv[])
{
    int opt;
    int scaleBy = 4;
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
        case 's':
            scaleBy = atoi(optarg);
            if (scaleBy != 2 && scaleBy != 3 && scaleBy != 4) {
                fprintf(stderr, "Only scale factors of 2, 3, and 4 are supported.");
                return 1;
            }
            break;
        default:
            goto error_usage;
        }
    }

    if (optind + 2 > argc) {
error_usage:
        fprintf(stderr, "Usage: %s [-s scaleBy] input output\n", argv[0]);
        return 1;
    }


    char *szFilenameIn = argv[optind];
    char *szFilenameOut = argv[optind + 1];


    int width, height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *in_row_pointers = NULL;
    LOG_INFO(" go here ");

    // load image;
    read_png_file(szFilenameIn, width, height, color_type, bit_depth, in_row_pointers ) ;

    LOG_INFO(" width %d %d %d %d ", width, height, color_type, bit_depth);


    //// Discard the alpha byte since the RGBtoYUV conversion
    //// expects the most significant byte to be empty
    //size_t i;
    //for (i = 3; i < srcSize; i += 4) {
    //    srcData[i] = 0;
    //}

	size_t srcSize = width * height * sizeof(uint32_t);
	uint8_t *srcData = (uint8_t *) malloc(srcSize);
	size_t destSize = width * scaleBy * height * scaleBy * sizeof(uint32_t);
	uint8_t *destData = (uint8_t *) malloc(destSize);


    for(int y  = 0;y< height ; y++){
        memcpy(&srcData[y*width*4], in_row_pointers[y], width*4);
    }


    uint32_t *sp = (uint32_t *) srcData;
    uint32_t *dp = (uint32_t *) destData;

    // If big endian we have to swap the byte order to get RGB values
    #ifdef WORDS_BIGENDIAN
    uint32_t *spTemp = sp;
    for (i = 0; i < srcSize >> 2; i++) {
        spTemp[i] = swapByteOrder(spTemp[i]);
    }
    #endif

    LOG_INFO(" go here");
    hqxInit();
    LOG_INFO(" go here");
    switch (scaleBy) {
    case 2:
        LOG_INFO(" go here");
        hq2x_32(sp, dp, width, height);
        LOG_INFO(" go here");
        break;
    case 3:
        hq3x_32(sp, dp, width, height);
        break;
    case 4:
    default:
        hq4x_32(sp, dp, width, height);
        break;
    }

    // If big endian we have to swap byte order of destData to get BGRA format
    #ifdef WORDS_BIGENDIAN
    uint32_t *dpTemp = dp;
    for (i = 0; i < destSize >> 2; i++) {
        dpTemp[i] = swapByteOrder(dpTemp[i]);
    }
    #endif

        LOG_INFO(" go here");
    png_bytep *out_row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height* scaleBy);
    for(int y = 0; y < height*scaleBy; y++) {
        out_row_pointers[y] = (png_byte*)malloc(4*width*scaleBy);
    }
        LOG_INFO(" go here");


        LOG_INFO(" go here");
    for(int y  = 0;y< height*scaleBy ; y++){
        memcpy(out_row_pointers[y], &destData[y*width*scaleBy*4],  width*scaleBy*4);
    }
        LOG_INFO(" go here");

    write_png_file(szFilenameOut, width*scaleBy, height*scaleBy, out_row_pointers ) ;
        LOG_INFO(" go here");

    for(int y = 0; y < height; y++) {
        free(in_row_pointers[y]);
    }
        LOG_INFO(" go here");
    if(in_row_pointers){ free(in_row_pointers);}
        LOG_INFO(" go here");

    out_row_pointers=NULL;


#if 0
	// Copy destData into image
	ilTexImage(width * scaleBy, height * scaleBy, 0, 4, IL_BGRA, IL_UNSIGNED_BYTE, destData);

    // Free image data
    free(srcData);
    free(destData);

    // Save image
	ilConvertImage(IL_BGR, IL_UNSIGNED_BYTE); // No alpha channel
	ilHint(IL_COMPRESSION_HINT, IL_USE_COMPRESSION);
    ilEnable(IL_FILE_OVERWRITE);
	ILboolean saved = ilSaveImage(szFilenameOut);

    ilDeleteImages(1, &handle);

    if (saved == IL_FALSE) {
        fprintf(stderr, "ERROR: can't save '%s'\n", szFilenameOut);
        return 1;
    }
#endif

    return 0;
}
