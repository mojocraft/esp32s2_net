/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --no-compress --stride 1 --align 1 --font 微软雅黑.ttf --symbols 欲买桂花同载酒终不似少年游 ,. --format lvgl -o yahei_14.c
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl.h"
#endif



#ifndef YAHEI_14
#define YAHEI_14 1
#endif

#if YAHEI_14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+002C "," */
    0x56,

    /* U+002E "." */
    0xf0,

    /* U+4E0D "不" */
    0xff, 0xf8, 0x18, 0x0, 0x80, 0xc, 0x0, 0xe8,
    0xd, 0x60, 0xc8, 0xdc, 0x43, 0x82, 0x0, 0x10,
    0x0, 0x80, 0x4, 0x0,

    /* U+4E70 "买" */
    0x7f, 0xf8, 0x8, 0x86, 0x44, 0x1a, 0x6, 0x10,
    0xc, 0x80, 0x4, 0x1f, 0xff, 0x2, 0x0, 0x3c,
    0x7, 0x19, 0xe0, 0x30, 0x0, 0x0,

    /* U+4F3C "似" */
    0x11, 0x90, 0x92, 0x42, 0x4d, 0x9, 0x4, 0x64,
    0x11, 0x90, 0x4a, 0x41, 0x9, 0xc, 0x25, 0xa0,
    0x9c, 0xc2, 0x45, 0x8, 0x32, 0x21, 0x88,

    /* U+540C "同" */
    0xff, 0xf8, 0x1, 0x80, 0x1b, 0xfd, 0x80, 0x19,
    0xf9, 0x90, 0x99, 0x9, 0x90, 0x99, 0xf9, 0x80,
    0x18, 0x1, 0x80, 0xf0,

    /* U+5C11 "少" */
    0x2, 0x0, 0x10, 0x4, 0x90, 0x64, 0x42, 0x21,
    0x21, 0x7, 0x8, 0x80, 0x48, 0x0, 0x80, 0x18,
    0x3, 0x80, 0xf0, 0xc, 0x0, 0x0,

    /* U+5E74 "年" */
    0x10, 0x0, 0x40, 0x3, 0xff, 0x98, 0x40, 0xc1,
    0x0, 0xff, 0xe2, 0x10, 0x8, 0x40, 0x21, 0x3,
    0xff, 0xf0, 0x10, 0x0, 0x40, 0x1, 0x0, 0x4,
    0x0,

    /* U+6842 "桂" */
    0x20, 0x40, 0x81, 0x2, 0x7f, 0xbe, 0x10, 0x20,
    0x41, 0x81, 0x7, 0x7f, 0xda, 0x10, 0xa0, 0x42,
    0x9f, 0xe2, 0x4, 0x8, 0x10, 0x27, 0xfc, 0x80,
    0x0,

    /* U+6B32 "欲" */
    0x28, 0x82, 0x24, 0x10, 0xff, 0x2, 0x13, 0x95,
    0x92, 0x21, 0x19, 0x10, 0x48, 0xfc, 0x42, 0x27,
    0x11, 0x28, 0x89, 0x67, 0xd1, 0x21, 0x84, 0x0,
    0x0,

    /* U+6E38 "游" */
    0x0, 0x2, 0x22, 0x9, 0x1e, 0x3f, 0x8, 0x88,
    0x24, 0x3c, 0x3c, 0x41, 0x24, 0x29, 0xfb, 0x49,
    0x12, 0x48, 0xb2, 0x45, 0x32, 0x3b, 0x70,

    /* U+7EC8 "终" */
    0x1, 0x0, 0x84, 0x2, 0x3f, 0x92, 0xc6, 0x5d,
    0x33, 0xd3, 0x82, 0xe, 0x8, 0xc7, 0x5c, 0x1,
    0xc3, 0x80, 0x3, 0xe, 0xc0, 0xc0, 0xf0, 0x0,
    0x40,

    /* U+82B1 "花" */
    0x8, 0x80, 0x44, 0x3f, 0xfe, 0x11, 0x1, 0x0,
    0xc, 0x80, 0xc4, 0x44, 0x2c, 0x61, 0xc7, 0x1c,
    0x2b, 0xc2, 0x52, 0x12, 0x10, 0x90, 0xf8,

    /* U+8F7D "载" */
    0x8, 0x80, 0x22, 0xc7, 0xe9, 0x82, 0x20, 0xff,
    0xfc, 0x42, 0xf, 0xf8, 0x88, 0x14, 0x48, 0x51,
    0xfd, 0x80, 0x86, 0x1f, 0xf9, 0xc9, 0xa4, 0x24,
    0x60,

    /* U+9152 "酒" */
    0x6f, 0xfc, 0xc4, 0x80, 0x12, 0x3, 0xfe, 0x69,
    0x28, 0xa4, 0xa0, 0xa3, 0x8b, 0x2, 0x2f, 0xf8,
    0xa0, 0x26, 0x80, 0x93, 0xfe, 0x48, 0x8
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 66, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 54, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 2, .adv_w = 54, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 224, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 23, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 45, .adv_w = 224, .box_w = 14, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 68, .adv_w = 224, .box_w = 12, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 88, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 110, .adv_w = 224, .box_w = 14, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 135, .adv_w = 224, .box_w = 14, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 160, .adv_w = 224, .box_w = 13, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 185, .adv_w = 224, .box_w = 13, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 208, .adv_w = 224, .box_w = 14, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 233, .adv_w = 224, .box_w = 13, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 256, .adv_w = 224, .box_w = 14, .box_h = 14, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 281, .adv_w = 224, .box_w = 14, .box_h = 13, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xc, 0xe, 0x4ded, 0x4e50, 0x4f1c, 0x53ec, 0x5bf1,
    0x5e54, 0x6822, 0x6b12, 0x6e18, 0x7ea8, 0x8291, 0x8f5d, 0x9132
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 37171, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 16, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t lv_font_yahei_14;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t yahei_14 = {
#else
lv_font_t yahei_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    //.static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &yahei_14,
#endif
    .user_data = NULL,
};



#endif /*#if YAHEI_14*/
